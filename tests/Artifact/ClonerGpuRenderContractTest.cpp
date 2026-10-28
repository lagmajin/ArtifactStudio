#include <gtest/gtest.h>

#include <Texture.h>
#include <TextureView.h>
#include <Buffer.h>
#include <DeviceContext.h>
#include <PipelineState.h>
#include <RenderDevice.h>
#include <Sampler.h>
#include <Shader.h>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFileInfo>
#include <QImage>
#include <QMatrix4x4>
#include <QRawFont>
#include <QRectF>
#include <QSize>
#include <QTemporaryDir>
#include <QUuid>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <utility>
#include <vector>
#include <future>
#include <thread>

#include "DiligentHeadlessRenderer.hpp"
import Color.Float;
import Graphics.ParticleData;
import Image.ImageF32x4_RGBA;
import Image.UploadConversion;
import Artifact.Render.DiligentImmediateSubmitter;
import Artifact.Render.ShaderManager;
import Artifact.Render.Config;
import Artifact.Render.Pipeline;
import Artifact.Layer.FluidRuntimeState;
import Core.ArtifactArray;
import Core.Simulation.Pyro;
import Render.VolumeRenderer;

namespace {

struct Bounds {
    int left;
    int top;
    int right;
    int bottom;
    std::size_t pixels;
};

std::vector<Bounds> findVisibleComponents(
    const ArtifactCore::ImageF32x4_RGBA& image)
{
    const int width = image.width();
    const int height = image.height();
    std::vector<std::uint8_t> visited(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    std::vector<std::size_t> pending;
    std::vector<Bounds> components;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto index = static_cast<std::size_t>(y) * width + x;
            if (visited[index] || image.getPixel(x, y).a() < 0.5f) {
                continue;
            }
            Bounds bounds{x, y, x, y, 0};
            pending.clear();
            pending.push_back(index);
            visited[index] = 1;
            while (!pending.empty()) {
                const auto current = pending.back();
                pending.pop_back();
                const int px = static_cast<int>(current % width);
                const int py = static_cast<int>(current / width);
                bounds.left = std::min(bounds.left, px);
                bounds.top = std::min(bounds.top, py);
                bounds.right = std::max(bounds.right, px);
                bounds.bottom = std::max(bounds.bottom, py);
                ++bounds.pixels;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = px + dx;
                        const int ny = py + dy;
                        if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 ||
                            nx >= width || ny >= height) {
                            continue;
                        }
                        const auto neighbor =
                            static_cast<std::size_t>(ny) * width + nx;
                        if (!visited[neighbor] &&
                            image.getPixel(nx, ny).a() >= 0.5f) {
                            visited[neighbor] = 1;
                            pending.push_back(neighbor);
                        }
                    }
                }
            }
            components.push_back(bounds);
        }
    }
    std::sort(components.begin(), components.end(),
              [](const Bounds& a, const Bounds& b) {
                  return a.left < b.left;
              });
    return components;
}

void appendMaximumMpmTestSurface(
    ArtifactCore::ArtifactArray<Artifact::MpmSurfaceVertex>& vertices)
{
    // Mirror the production MPM grid's maximum (129 * 97 cells * 6 vertices).
    // The submitter stages 4096 vertices, so the final triangle checks the
    // repeated bounded flush path instead of only a single triangle upload.
    constexpr std::size_t maximumVertices = 129u * 97u * 6u;
    constexpr std::size_t maximumTriangles = maximumVertices / 3u;
    constexpr std::size_t firstRunTriangles = 4096u / 3u;
    vertices.reserve(maximumVertices);
    for (std::size_t triangle = 0; triangle < maximumTriangles; ++triangle) {
        if (triangle < firstRunTriangles) {
            vertices.append({12.0f, 12.0f, 1.0f});
            vertices.append({32.0f, 12.0f, 1.0f});
            vertices.append({22.0f, 32.0f, 1.0f});
        } else if (triangle + 1u == maximumTriangles) {
            vertices.append({72.0f, 12.0f, 1.0f});
            vertices.append({92.0f, 12.0f, 1.0f});
            vertices.append({82.0f, 32.0f, 1.0f});
        } else {
            vertices.append({42.0f, 12.0f, 1.0f});
            vertices.append({62.0f, 12.0f, 1.0f});
            vertices.append({52.0f, 32.0f, 1.0f});
        }
    }
    EXPECT_EQ(vertices.size(), maximumVertices);
}

template <typename DrawFrame>
void expectDeferredVideoTextureReuseOrder(
    ArtifactTest::HeadlessRenderer& renderer, const QUuid& videoLayerId,
    const QString& capturePrefix, int probeX, int probeY,
    DrawFrame&& drawFrame)
{
    ArtifactCore::ImageF32x4_RGBA redFrame;
    redFrame.resize(4, 4);
    redFrame.fill(ArtifactCore::FloatRGBA(0.95f, 0.05f, 0.05f, 1.0f));
    ArtifactCore::ImageF32x4_RGBA blueFrame;
    blueFrame.resize(4, 4);
    blueFrame.fill(ArtifactCore::FloatRGBA(0.05f, 0.05f, 0.95f, 1.0f));

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    auto* redTexture = renderer.textureForVideoFrame(
        redFrame, videoLayerId, 1);
    ASSERT_NE(redTexture, nullptr);
    ASSERT_TRUE(drawFrame(redTexture));
    renderer.flushAndWait();
    const auto synchronousRedFrame = renderer.readbackToImageF32();

    renderer.clear();
    redTexture = renderer.textureForVideoFrame(redFrame, videoLayerId, 1);
    ASSERT_NE(redTexture, nullptr);
    ASSERT_TRUE(drawFrame(redTexture));
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the selected packet mix should be recorded asynchronously";

    // Reuse the same slot while the red command list is still pending on the
    // owner. The overwrite callback must execute that list before blue upload.
    auto* blueTexture = renderer.textureForVideoFrame(
        blueFrame, videoLayerId, 2);
    ASSERT_NE(blueTexture, nullptr);
    EXPECT_EQ(blueTexture, redTexture);
    renderer.flushAndWait();

    const auto deferredRedFrame = renderer.readbackToImageF32();
    float maxRedFrameError = 0.0f;
    for (int y = 0; y < deferredRedFrame.height(); ++y) {
        for (int x = 0; x < deferredRedFrame.width(); ++x) {
            const auto expected = synchronousRedFrame.getPixel(x, y);
            const auto actual = deferredRedFrame.getPixel(x, y);
            maxRedFrameError = std::max({
                maxRedFrameError,
                std::abs(actual.r() - expected.r()),
                std::abs(actual.g() - expected.g()),
                std::abs(actual.b() - expected.b()),
                std::abs(actual.a() - expected.a())});
        }
    }
    EXPECT_LE(maxRedFrameError, 1.0e-4f)
        << "the earlier packet must sample its red frame before slot reuse";

    renderer.clear();
    blueTexture = renderer.textureForVideoFrame(blueFrame, videoLayerId, 2);
    ASSERT_NE(blueTexture, nullptr);
    ASSERT_TRUE(drawFrame(blueTexture));
    renderer.flushAndWait();
    const auto deferredBlueFrame = renderer.readbackToImageF32();
    const auto redCenter = synchronousRedFrame.getPixel(probeX, probeY);
    const auto blueCenter = deferredBlueFrame.getPixel(probeX, probeY);
    const float colorDifference = std::abs(blueCenter.r() - redCenter.r()) +
        std::abs(blueCenter.g() - redCenter.g()) +
        std::abs(blueCenter.b() - redCenter.b());
    EXPECT_GT(colorDifference, 0.5f)
        << "the following frame must sample the updated blue slot";

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    ASSERT_TRUE(deferredRedFrame.toQImage().save(
        QDir(captureDirectory).filePath(
            QStringLiteral("%1_first_%2.png").arg(capturePrefix, backend)),
        "PNG"));
    ASSERT_TRUE(deferredBlueFrame.toQImage().save(
        QDir(captureDirectory).filePath(
            QStringLiteral("%1_second_%2.png").arg(capturePrefix, backend)),
        "PNG"));
}

} // namespace

TEST(ClonerGpuRenderContractTest,
     DiligentHeadlessPlacementBufferRendersThreeScaledSprites)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128, true)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    if (!renderer.clonerGridPlacementGpuReady()) {
        GTEST_SKIP() << "Diligent backend could not create the Cloner GPU pipelines";
    }

    auto device = renderer.device();
    ASSERT_NE(device, nullptr);
    constexpr std::array<std::uint8_t, 4> texel{255, 255, 255, 255};
    Diligent::TextureDesc textureDesc;
    textureDesc.Name = "Cloner GPU render contract source";
    textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    textureDesc.Usage = Diligent::USAGE_IMMUTABLE;
    textureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData subresource;
    subresource.pData = texel.data();
    subresource.Stride = texel.size();
    Diligent::TextureData initialData;
    initialData.pSubResources = &subresource;
    initialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> sourceTexture;
    device->CreateTexture(textureDesc, &initialData, &sourceTexture);
    ASSERT_NE(sourceTexture, nullptr);
    auto* sourceView = sourceTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(sourceView, nullptr);

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    // Keep the following dispatch and instance draws together in one packet
    // recording pass, while crossing the submitter's 16-packet deferred path.
    for (int index = 0; index < 12; ++index) {
        renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                               -40.0f, 2.0f, 2.0f,
                               ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    }

    Artifact::ClonerGridPlacementRange placementRange;
    ASSERT_TRUE(renderer.dispatchClonerGridPlacement(
        2, 1, 1, 40.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.75f,
        placementRange));
    ASSERT_EQ(placementRange.count, 3u);
    ASSERT_TRUE(placementRange.rotationAware);

    QMatrix4x4 identity;
    identity.setToIdentity();
    for (std::uint32_t index = 0; index < placementRange.count; ++index) {
        ASSERT_TRUE(renderer.drawSpriteTransformedClonerGrid(
            64.0f, 64.0f, 12.0f, 8.0f, identity, identity, sourceView,
            1.0f, placementRange,
            placementRange.firstIndex + index)) << "placement=" << index;
    }
    renderer.flushAndWait();

    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), 128);
    ASSERT_EQ(image.height(), 128);
    const auto components = findVisibleComponents(image);
    ASSERT_EQ(components.size(), 3u);
    constexpr std::array<float, 3> expectedCenterX{53.0f, 70.0f, 93.0f};
    for (std::size_t index = 0; index < components.size(); ++index) {
        const auto& component = components[index];
        EXPECT_GT(component.pixels, 40u);
        EXPECT_LT(component.pixels, 400u);
        EXPECT_NEAR((component.left + component.right) * 0.5f,
                    expectedCenterX[index],
                    1.5f);
        EXPECT_NEAR((component.top + component.bottom) * 0.5f, 67.0f, 1.5f);
    }
}

TEST(ClonerGpuRenderContractTest,
     DeferredSubmitterPreservesInterleavedQuadAndTrianglePacketOrder)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();

    // The first 14 packets are invisible padding. Together with the two
    // overlapping draws below, they cross DiligentImmediateSubmitter's
    // deferred-recording threshold (16 packets).
    for (int index = 0; index < 14; ++index) {
        renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                               -40.0f, 2.0f, 2.0f,
                               ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // Triangle first, overlapping quad second: the center pixel must retain
    // the later quad's green color. Flushing primitive batches by type at the
    // end of submit used to draw the triangle after the quad and turn it red.
    renderer.drawSolidTriangleLocal(Artifact::Detail::float2(0.0f, 0.0f),
                                    Artifact::Detail::float2(128.0f, 0.0f),
                                    Artifact::Detail::float2(64.0f, 128.0f),
                                    ArtifactCore::FloatColor(1.0f, 0.0f, 0.0f, 1.0f));
    renderer.drawSolidRect(16.0f, 58.0f, 96.0f, 12.0f,
                           ArtifactCore::FloatColor(0.0f, 1.0f, 0.0f, 1.0f));
    renderer.flushAndWait();

    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), 128);
    ASSERT_EQ(image.height(), 128);
    const auto center = image.getPixel(64, 64);
    EXPECT_LT(center.r(), 0.2f);
    EXPECT_GT(center.g(), 0.8f);
    EXPECT_LT(center.b(), 0.2f);
}

TEST(ClonerGpuRenderContractTest,
     DeferredSubmitPreservesPartialViewportAndScissor)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }

    const auto renderViewport = [&](bool crossDeferredThreshold) {
        renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
        renderer.clear();
        renderer.setViewportRect(32.0f, 24.0f, 64.0f, 80.0f, 128.0f, 128.0f);
        if (crossDeferredThreshold) {
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                       -40.0f, 2.0f, 2.0f,
                                       ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
        renderer.drawSolidRect(0.0f, 0.0f, 64.0f, 80.0f,
                               ArtifactCore::FloatColor(0.9f, 0.1f, 0.05f, 1.0f));
        renderer.flushAndWait();
        return renderer.readbackToImageF32();
    };

    const auto immediate = renderViewport(false);
    const auto deferred = renderViewport(true);
    ASSERT_EQ(immediate.width(), 128);
    ASSERT_EQ(immediate.height(), 128);
    ASSERT_EQ(deferred.width(), 128);
    ASSERT_EQ(deferred.height(), 128);

    std::size_t coverageMismatches = 0;
    float maxChannelDifference = 0.0f;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const auto immediatePixel = immediate.getPixel(x, y);
            const auto deferredPixel = deferred.getPixel(x, y);
            const bool immediateCovered = immediatePixel.a() > 0.5f;
            const bool deferredCovered = deferredPixel.a() > 0.5f;
            coverageMismatches += immediateCovered != deferredCovered ? 1u : 0u;
            maxChannelDifference = std::max({
                maxChannelDifference,
                std::abs(immediatePixel.r() - deferredPixel.r()),
                std::abs(immediatePixel.g() - deferredPixel.g()),
                std::abs(immediatePixel.b() - deferredPixel.b()),
                std::abs(immediatePixel.a() - deferredPixel.a())});
        }
    }
    EXPECT_EQ(coverageMismatches, 0u);
    EXPECT_LE(maxChannelDifference, 1.0e-4f);
    EXPECT_GT(deferred.getPixel(64, 64).r(), 0.7f);
    EXPECT_GT(deferred.getPixel(64, 64).a(), 0.9f);
    EXPECT_LT(deferred.getPixel(16, 16).a(), 0.1f);
    EXPECT_LT(deferred.getPixel(110, 110).a(), 0.1f);
}

TEST(ClonerGpuRenderContractTest,
     DeferredSubmitDrainsPacketsBeforeViewportSwitch)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto renderTwoViewports = [&](bool crossDeferredThreshold) {
        renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
        renderer.clear();
        renderer.setViewportRect(16.0f, 16.0f, 48.0f, 48.0f, 128.0f, 128.0f);
        if (crossDeferredThreshold) {
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                       -40.0f, 2.0f, 2.0f,
                                       ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
        renderer.drawSolidRect(0.0f, 0.0f, 48.0f, 48.0f,
                               ArtifactCore::FloatColor(0.9f, 0.05f, 0.05f, 1.0f));

        // Changing viewport state is an order boundary: the first batch must
        // be submitted with its captured viewport before the second is set.
        renderer.setViewportRect(64.0f, 64.0f, 48.0f, 48.0f, 128.0f, 128.0f);
        if (crossDeferredThreshold) {
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                       -40.0f, 2.0f, 2.0f,
                                       ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
        renderer.drawSolidRect(0.0f, 0.0f, 48.0f, 48.0f,
                               ArtifactCore::FloatColor(0.05f, 0.1f, 0.9f, 1.0f));
        renderer.flushAndWait();
        const auto image = renderer.readbackToImageF32();
        const auto capturePath = QDir(captureDirectory).filePath(
            QStringLiteral("artifact_renderer_%1_%2.png")
                .arg(crossDeferredThreshold ? QStringLiteral("deferred")
                                            : QStringLiteral("immediate"),
                     backend));
        EXPECT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
            << capturePath.toStdString();
        return image;
    };

    const quint64 workerSubmitCountBeforeImmediate =
        renderer.deferredWorkerSubmitCount();
    const auto immediate = renderTwoViewports(false);
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
              workerSubmitCountBeforeImmediate)
        << "small batches should remain on the immediate submit path";
    const quint64 workerSubmitCountBeforeDeferred =
        renderer.deferredWorkerSubmitCount();
    const auto deferred = renderTwoViewports(true);
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
              workerSubmitCountBeforeDeferred + 2)
        << "the two threshold-sized production renderer batches should be recorded on its worker";
    ASSERT_EQ(immediate.width(), 128);
    ASSERT_EQ(immediate.height(), 128);
    ASSERT_EQ(deferred.width(), 128);
    ASSERT_EQ(deferred.height(), 128);

    std::size_t coverageMismatches = 0;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const bool immediateCovered = immediate.getPixel(x, y).a() > 0.5f;
            const bool deferredCovered = deferred.getPixel(x, y).a() > 0.5f;
            coverageMismatches += immediateCovered != deferredCovered ? 1u : 0u;
        }
    }
    EXPECT_EQ(coverageMismatches, 0u);
    const auto firstViewportCenter = deferred.getPixel(40, 40);
    EXPECT_GT(firstViewportCenter.r(), 0.7f);
    EXPECT_GT(firstViewportCenter.a(), 0.9f);
    const auto secondViewportCenter = deferred.getPixel(88, 88);
    EXPECT_GT(secondViewportCenter.b(), 0.7f);
    EXPECT_GT(secondViewportCenter.a(), 0.9f);
    EXPECT_LT(deferred.getPixel(40, 88).a(), 0.1f);

}

TEST(ClonerGpuRenderContractTest,
     DeferredCommandListCanBeRecordedAndFinishedOnWorker)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }

    auto device = renderer.device();
    auto immediate = renderer.immediateContext();
    ASSERT_NE(device, nullptr);
    ASSERT_NE(immediate, nullptr);
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deferred;
    device->CreateDeferredContext(&deferred);
    ASSERT_NE(deferred, nullptr);

    Artifact::ShaderManager shaders(device, Artifact::RenderConfig::MainRTVFormat);
    ASSERT_TRUE(shaders.isInitialized());
    shaders.createShaders();
    shaders.createPSOs();
    Artifact::DiligentImmediateSubmitter submitter;
    submitter.createBuffers(device, Artifact::RenderConfig::MainRTVFormat);
    submitter.setPSOs(shaders);
    Artifact::DiligentImmediateSubmitter workerSubmitter;
    // This recorder has its own buffers and SRBs; prepare them before its
    // thread starts and never share them with the immediate submitter.
    workerSubmitter.createBuffers(device, Artifact::RenderConfig::MainRTVFormat);
    workerSubmitter.setPSOs(shaders);
    constexpr std::array<std::uint8_t, 4> spriteTexel{255, 255, 255, 255};
    Diligent::TextureDesc spriteTextureDesc;
    spriteTextureDesc.Name = "Deferred worker sprite contract";
    spriteTextureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    spriteTextureDesc.Width = 1;
    spriteTextureDesc.Height = 1;
    spriteTextureDesc.MipLevels = 1;
    spriteTextureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    spriteTextureDesc.Usage = Diligent::USAGE_IMMUTABLE;
    spriteTextureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData spriteSubresource;
    spriteSubresource.pData = spriteTexel.data();
    spriteSubresource.Stride = spriteTexel.size();
    Diligent::TextureData spriteInitialData;
    spriteInitialData.pSubResources = &spriteSubresource;
    spriteInitialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> spriteTexture;
    device->CreateTexture(spriteTextureDesc, &spriteInitialData,
                          &spriteTexture);
    ASSERT_NE(spriteTexture, nullptr);
    auto* spriteSRV = spriteTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(spriteSRV, nullptr);
    const auto makeBuffer = [&] {
        Artifact::RenderCommandBuffer buffer;
        buffer.reserve(29);
        buffer.setTargetRTV(renderer.layerRenderTargetView());
        for (int index = 0; index < 16; ++index) {
            if (index < 12) {
                Artifact::SolidRectPkt packet{};
                packet.xform.offset = {-40.0f - static_cast<float>(index) * 3.0f,
                                       -40.0f};
                packet.xform.scale = {2.0f, 2.0f};
                packet.xform.screenSize = {128.0f, 128.0f};
                packet.color = {1.0f, 1.0f, 1.0f, 1.0f};
                buffer.append(packet);
                continue;
            }
            if (index == 12) {
                Artifact::SolidTriPkt packet{};
                packet.xform.scale = {1.0f, 1.0f};
                packet.xform.screenSize = {128.0f, 128.0f};
                packet.p0 = {0.0f, 0.0f};
                packet.p1 = {128.0f, 0.0f};
                packet.p2 = {64.0f, 128.0f};
                packet.color = {1.0f, 0.0f, 0.0f, 1.0f};
                buffer.append(packet);
                continue;
            }
            if (index == 13) {
                Artifact::LinePkt packet{};
                packet.xform.scale = {1.0f, 1.0f};
                packet.xform.screenSize = {128.0f, 128.0f};
                packet.p1 = {8.0f, 8.0f};
                packet.p2 = {48.0f, 8.0f};
                packet.c1 = packet.c2 = {0.1f, 0.2f, 1.0f, 1.0f};
                buffer.append(packet);
                continue;
            }
            if (index == 14) {
                Artifact::QuadPkt packet{};
                packet.xform.scale = {1.0f, 1.0f};
                packet.xform.screenSize = {128.0f, 128.0f};
                packet.p0 = {80.0f, 8.0f};
                packet.p1 = {112.0f, 8.0f};
                packet.p2 = {112.0f, 40.0f};
                packet.p3 = {80.0f, 40.0f};
                packet.color = {1.0f, 1.0f, 0.0f, 1.0f};
                buffer.append(packet);
                continue;
            }
            Artifact::SolidRectPkt packet{};
            packet.xform.offset = {16.0f, 58.0f};
            packet.xform.scale = {96.0f, 12.0f};
            packet.xform.screenSize = {128.0f, 128.0f};
            packet.color = {0.0f, 1.0f, 0.0f, 1.0f};
            buffer.append(packet);
        }
        Artifact::SpritePkt sprite{};
        sprite.xform.offset = {8.0f, 80.0f};
        sprite.xform.scale = {24.0f, 24.0f};
        sprite.xform.screenSize = {128.0f, 128.0f};
        sprite.pSRV = spriteSRV;
        buffer.append(sprite);

        Artifact::SpriteXformPkt transformedSprite{};
        transformedSprite.mat.row0 = {0.375f, 0.0f, 0.0f, 0.125f};
        transformedSprite.mat.row1 = {0.0f, -0.375f, 0.0f, -0.125f};
        transformedSprite.mat.row2 = {0.0f, 0.0f, 1.0f, 0.0f};
        transformedSprite.mat.row3 = {0.0f, 0.0f, 0.0f, 1.0f};
        transformedSprite.pSRV = spriteSRV;
        buffer.append(transformedSprite);

        Artifact::TexturedTriangleXformPkt texturedTriangle{};
        texturedTriangle.mat.row0 = {0.375f, 0.0f, 0.0f, 0.5f};
        texturedTriangle.mat.row1 = {0.0f, -0.375f, 0.0f, -0.25f};
        texturedTriangle.mat.row2 = {0.0f, 0.0f, 1.0f, 0.0f};
        texturedTriangle.mat.row3 = {0.0f, 0.0f, 0.0f, 1.0f};
        texturedTriangle.p0 = {0.0f, 0.0f};
        texturedTriangle.p1 = {1.0f, 0.0f};
        texturedTriangle.p2 = {0.0f, 1.0f};
        texturedTriangle.uv0 = {0.0f, 0.0f};
        texturedTriangle.uv1 = {1.0f, 0.0f};
        texturedTriangle.uv2 = {0.0f, 1.0f};
        texturedTriangle.pSRV = spriteSRV;
        buffer.append(texturedTriangle);

        Artifact::MaskedSpritePkt maskedSprite{};
        maskedSprite.xform.offset = {4.0f, 40.0f};
        maskedSprite.xform.scale = {20.0f, 16.0f};
        maskedSprite.xform.screenSize = {128.0f, 128.0f};
        maskedSprite.sceneSRV = spriteSRV;
        maskedSprite.maskSRV = spriteSRV;
        buffer.append(maskedSprite);

        Artifact::AtlasSpritePkt atlasSprite{};
        atlasSprite.xform.offset = {40.0f, 80.0f};
        atlasSprite.xform.scale = {24.0f, 24.0f};
        atlasSprite.xform.screenSize = {128.0f, 128.0f};
        atlasSprite.pSRV = spriteSRV;
        atlasSprite.uvRect = {0.0f, 0.0f, 1.0f, 1.0f};
        atlasSprite.color = {1.0f, 1.0f, 1.0f, 1.0f};
        buffer.append(atlasSprite);

        Artifact::AtlasSpriteXformPkt transformedAtlasSprite{};
        transformedAtlasSprite.mat.row0 = {0.375f, 0.0f, 0.0f, 0.5625f};
        transformedAtlasSprite.mat.row1 = {0.0f, -0.375f, 0.0f, -0.25f};
        transformedAtlasSprite.mat.row2 = {0.0f, 0.0f, 1.0f, 0.0f};
        transformedAtlasSprite.mat.row3 = {0.0f, 0.0f, 0.0f, 1.0f};
        transformedAtlasSprite.pSRV = spriteSRV;
        transformedAtlasSprite.uvRect = {0.0f, 0.0f, 1.0f, 1.0f};
        transformedAtlasSprite.color = {1.0f, 1.0f, 1.0f, 1.0f};
        buffer.append(transformedAtlasSprite);

        Artifact::GradientRectPkt gradient{};
        gradient.mat.row0 = {0.375f, 0.0f, 0.0f, -0.75f};
        gradient.mat.row1 = {0.0f, -0.375f, 0.0f, -0.625f};
        gradient.mat.row2 = {0.0f, 0.0f, 1.0f, 0.0f};
        gradient.mat.row3 = {0.0f, 0.0f, 0.0f, 1.0f};
        gradient.params.startColor = {0.9f, 0.1f, 0.2f, 1.0f};
        gradient.params.endColor = {0.1f, 0.2f, 0.9f, 1.0f};
        gradient.params.mode = {1.0f, 0.0f, 0.0f, 1.0f};
        gradient.params.colorContract = {1.0f, 0.0f, 0.0f, 0.0f};
        gradient.opacity = 1.0f;
        buffer.append(gradient);

        Artifact::DotLinePkt dotLine{};
        dotLine.xform.scale = {1.0f, 1.0f};
        dotLine.xform.screenSize = {128.0f, 128.0f};
        dotLine.verts[0] = {{100.0f, 108.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, 0.0f, 0.0f};
        dotLine.verts[1] = {{124.0f, 108.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, 24.0f, 0.0f};
        dotLine.verts[2] = {{100.0f, 112.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, 0.0f, 0.0f};
        dotLine.verts[3] = {{124.0f, 112.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, 24.0f, 0.0f};
        dotLine.thickness = 4.0f;
        dotLine.spacing = 6.0f;
        buffer.append(dotLine);

        Artifact::CheckerboardPkt checkerboard{};
        checkerboard.xform.offset = {4.0f, 104.0f};
        checkerboard.xform.scale = {28.0f, 20.0f};
        checkerboard.xform.screenSize = {128.0f, 128.0f};
        checkerboard.helper.param0 = 4.0f;
        checkerboard.helper.color1 = {0.0f, 1.0f, 1.0f, 1.0f};
        checkerboard.helper.color2 = {0.0f, 0.2f, 0.2f, 1.0f};
        checkerboard.baseColor = {0.05f, 0.05f, 0.05f, 1.0f};
        buffer.append(checkerboard);

        Artifact::GridPkt grid{};
        grid.xform.offset = {36.0f, 104.0f};
        grid.xform.scale = {28.0f, 20.0f};
        grid.xform.screenSize = {128.0f, 128.0f};
        grid.helper.param0 = 6.0f;
        grid.helper.param1 = 1.0f;
        grid.helper.color1 = {0.1f, 1.0f, 0.1f, 1.0f};
        grid.baseColor = {0.1f, 0.1f, 0.1f, 1.0f};
        buffer.append(grid);

        Artifact::RectOutlinePkt outline{};
        outline.xform.offset = {68.0f, 104.0f};
        outline.xform.scale = {28.0f, 20.0f};
        outline.xform.screenSize = {128.0f, 128.0f};
        outline.color = {0.2f, 0.3f, 1.0f, 1.0f};
        buffer.append(outline);

        Artifact::SolidCirclePkt circle{};
        circle.xform.scale = {1.0f, 1.0f};
        circle.xform.screenSize = {128.0f, 128.0f};
        circle.cx = 96.0f;
        circle.cy = 64.0f;
        circle.radius = 9.0f;
        circle.color = {0.9f, 0.1f, 0.8f, 1.0f};
        buffer.append(circle);

        Artifact::MpmSurfacePkt mpmSurface{};
        mpmSurface.xform.scale = {1.0f, 1.0f};
        mpmSurface.xform.screenSize = {128.0f, 128.0f};
        mpmSurface.color = {0.1f, 0.9f, 0.25f, 1.0f};
        appendMaximumMpmTestSurface(mpmSurface.vertices);
        buffer.append(std::move(mpmSurface));
        return buffer;
    };

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    Artifact::RenderCommandBuffer immediateBuffer = makeBuffer();
    submitter.submit(immediateBuffer, immediate.RawPtr());
    renderer.flushAndWait();
    const auto immediateImage = renderer.readbackToImageF32();
    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto immediateCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("deferred_worker_immediate_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(immediateCapturePath, "PNG"))
        << immediateCapturePath.toStdString();

    renderer.clear();
    submitter.setDeferredContext(deferred);
    Artifact::RenderCommandBuffer buffer = makeBuffer();

    Diligent::RefCntAutoPtr<Diligent::ICommandList> commandList;
    const auto immediateContextId = immediate->GetDesc().ContextId;
    std::promise<bool> recordResult;
    std::promise<void> executeCompleted;
    auto result = recordResult.get_future();
    auto execution = executeCompleted.get_future();
    std::thread worker([&] {
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> workerDeferred;
        device->CreateDeferredContext(&workerDeferred);
        if (!workerDeferred) {
            recordResult.set_value(false);
            return;
        }
        workerSubmitter.setDeferredContext(workerDeferred);
        const bool recorded = workerSubmitter.recordDeferred(
            buffer, immediateContextId, commandList);
        recordResult.set_value(recorded);
        if (recorded) {
            execution.wait();
            workerSubmitter.finishDeferredFrame();
        }
    });

    const bool recorded = result.get();
    if (recorded) {
        if (commandList) {
            auto* rawCommandList = commandList.RawPtr();
            immediate->ExecuteCommandLists(1, &rawCommandList);
        }
        executeCompleted.set_value();
    }
    worker.join();
    EXPECT_TRUE(recorded);
    EXPECT_NE(commandList, nullptr);
    EXPECT_EQ(buffer.packets().size(), 29u);
    renderer.flushAndWait();
    const auto workerImage = renderer.readbackToImageF32();
    ASSERT_EQ(immediateImage.width(), workerImage.width());
    ASSERT_EQ(immediateImage.height(), workerImage.height());
    std::size_t coverageMismatches = 0;
    std::size_t coveredPixels = 0;
    float maxChannelDifference = 0.0f;
    for (int y = 0; y < workerImage.height(); ++y) {
        for (int x = 0; x < workerImage.width(); ++x) {
            const auto immediatePixel = immediateImage.getPixel(x, y);
            const auto workerPixel = workerImage.getPixel(x, y);
            const bool immediateCovered = immediatePixel.a() > 0.5f;
            const bool workerCovered = workerPixel.a() > 0.5f;
            coverageMismatches += immediateCovered != workerCovered ? 1u : 0u;
            coveredPixels += workerCovered ? 1u : 0u;
            maxChannelDifference = std::max({
                maxChannelDifference,
                std::abs(immediatePixel.r() - workerPixel.r()),
                std::abs(immediatePixel.g() - workerPixel.g()),
                std::abs(immediatePixel.b() - workerPixel.b()),
                std::abs(immediatePixel.a() - workerPixel.a())});
        }
    }
    EXPECT_EQ(coverageMismatches, 0u);
    EXPECT_GT(coveredPixels, 0u);
    EXPECT_LE(maxChannelDifference, 1.0e-4f);
    const auto linePixel = workerImage.getPixel(24, 8);
    EXPECT_GT(linePixel.r(), 0.5f);
    const auto quadPixel = workerImage.getPixel(108, 20);
    EXPECT_GT(quadPixel.r(), 0.5f);
    EXPECT_GT(quadPixel.g(), 0.5f);
    const auto center = workerImage.getPixel(64, 64);
    EXPECT_LT(center.r(), 0.2f);
    EXPECT_GT(center.g(), 0.8f);
    EXPECT_LT(center.b(), 0.2f);
    const auto circleCenter = workerImage.getPixel(96, 64);
    EXPECT_GT(circleCenter.r(), 0.8f);
    EXPECT_LT(circleCenter.g(), 0.4f);
    EXPECT_GT(circleCenter.b(), 0.7f);
    const auto texturedTrianglePixel = workerImage.getPixel(102, 86);
    EXPECT_GT(texturedTrianglePixel.r(), 0.8f);
    EXPECT_GT(texturedTrianglePixel.g(), 0.8f);
    EXPECT_GT(texturedTrianglePixel.b(), 0.8f);
    const auto maskedSpritePixel = workerImage.getPixel(10, 46);
    EXPECT_GT(maskedSpritePixel.r(), 0.8f);
    EXPECT_GT(maskedSpritePixel.g(), 0.8f);
    EXPECT_GT(maskedSpritePixel.b(), 0.8f);
    const auto mpmSurfaceCenter = workerImage.getPixel(22, 18);
    EXPECT_GT(mpmSurfaceCenter.r(), 0.05f);
    EXPECT_GT(mpmSurfaceCenter.g(), 0.75f);
    EXPECT_GT(mpmSurfaceCenter.b(), 0.45f);
    EXPECT_LT(mpmSurfaceCenter.b(), 0.65f);
    for (const int x : {52, 82}) {
        const auto spillTriangleCenter = workerImage.getPixel(x, 18);
        EXPECT_GT(spillTriangleCenter.g(), 0.75f) << "x=" << x;
    }
    const auto plainSpriteCenter = workerImage.getPixel(20, 90);
    EXPECT_GT(plainSpriteCenter.a(), 0.8f);
    EXPECT_GT(plainSpriteCenter.r(), 0.8f);
    const auto transformedSpriteCenter = workerImage.getPixel(88, 80);
    EXPECT_GT(transformedSpriteCenter.a(), 0.8f);
    EXPECT_GT(transformedSpriteCenter.r(), 0.8f);
    const auto atlasSpriteCenter = workerImage.getPixel(52, 90);
    EXPECT_GT(atlasSpriteCenter.a(), 0.8f);
    EXPECT_GT(atlasSpriteCenter.r(), 0.8f);
    const auto transformedAtlasSpriteCenter = workerImage.getPixel(112, 90);
    EXPECT_GT(transformedAtlasSpriteCenter.a(), 0.8f);
    EXPECT_GT(transformedAtlasSpriteCenter.r(), 0.8f);
    const auto workerCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("deferred_worker_recorded_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(workerCapturePath, "PNG"))
        << workerCapturePath.toStdString();

    renderer.clear();
    Artifact::RenderCommandBuffer productionDeferredBuffer = makeBuffer();
    submitter.submit(productionDeferredBuffer, immediate.RawPtr());
    renderer.flushAndWait();
    const auto productionDeferredImage = renderer.readbackToImageF32();
    ASSERT_EQ(immediateImage.width(), productionDeferredImage.width());
    ASSERT_EQ(immediateImage.height(), productionDeferredImage.height());
    float productionMaxChannelDifference = 0.0f;
    for (int y = 0; y < productionDeferredImage.height(); ++y) {
        for (int x = 0; x < productionDeferredImage.width(); ++x) {
            const auto referencePixel = immediateImage.getPixel(x, y);
            const auto deferredPixel = productionDeferredImage.getPixel(x, y);
            productionMaxChannelDifference = std::max({
                productionMaxChannelDifference,
                std::abs(referencePixel.r() - deferredPixel.r()),
                std::abs(referencePixel.g() - deferredPixel.g()),
                std::abs(referencePixel.b() - deferredPixel.b()),
                std::abs(referencePixel.a() - deferredPixel.a())});
        }
    }
    EXPECT_LE(productionMaxChannelDifference, 1.0e-4f);
    const auto productionDeferredCapturePath = QDir(captureDirectory).filePath(
        QStringLiteral("deferred_submit_recorded_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(productionDeferredCapturePath, "PNG"))
        << productionDeferredCapturePath.toStdString();
    commandList = nullptr;
    buffer.reset();
    submitter.destroy();
}

TEST(ClonerGpuRenderContractTest,
     DeferredWorkerFallbackPreservesPendingGizmoOverlay)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    for (int index = 0; index < 2; ++index) {
        renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                               -40.0f, 2.0f, 2.0f,
                               ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    }
    renderer.drawSolidRect(0.0f, 0.0f, 48.0f, 48.0f,
                           ArtifactCore::FloatColor(0.9f, 0.05f, 0.05f, 1.0f));
    renderer.drawGizmoLine({-0.75f, 0.0f, 0.0f}, {0.75f, 0.0f, 0.0f},
                           ArtifactCore::FloatColor(0.05f, 0.95f, 0.1f, 1.0f));

    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore)
        << "the worker's 2D-only recorder must not consume a batch with pending 3D gizmo vertices";

    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), 128);
    ASSERT_EQ(image.height(), 128);
    const auto center = image.getPixel(40, 40);
    EXPECT_GT(center.r(), 0.7f);
    EXPECT_GT(center.a(), 0.9f);
    bool foundGizmoGreen = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.getPixel(x, y);
            foundGizmoGreen = foundGizmoGreen ||
                (pixel.g() > 0.6f && pixel.r() < 0.4f && pixel.a() > 0.5f);
        }
    }
    EXPECT_TRUE(foundGizmoGreen)
        << "the pending gizmo line should remain visible after the fallback submission";

    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_pending_gizmo_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();

}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererAsyncDeferredRecordingPreservesGizmoOrder)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    const ArtifactCore::FloatColor clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const ArtifactCore::FloatColor invisibleColor(1.0f, 1.0f, 1.0f, 1.0f);
    const ArtifactCore::FloatColor baseColor(0.95f, 0.1f, 0.1f, 1.0f);
    const ArtifactCore::FloatColor overlayColor(0.1f, 0.2f, 0.95f, 1.0f);
    const ArtifactCore::FloatColor gizmoColor(0.05f, 0.95f, 0.1f, 1.0f);

    const auto drawBase = [&](bool addWorkerThresholdFillers) {
        if (addWorkerThresholdFillers) {
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(
                    -40.0f - static_cast<float>(index) * 3.0f, -40.0f,
                    2.0f, 2.0f, invisibleColor);
            }
        }
        renderer.drawSolidRect(16.0f, 16.0f, 64.0f, 64.0f, baseColor);
        renderer.drawGizmoLine({-0.75f, 0.0f, 0.0f}, {0.75f, 0.0f, 0.0f},
                               gizmoColor);
    };
    const auto drawOverlay = [&] {
        renderer.drawSolidRect(48.0f, 48.0f, 32.0f, 32.0f, overlayColor);
    };

    renderer.setClearColor(clearColor);
    renderer.clear();
    drawBase(false);
    renderer.flushAndWait();
    drawOverlay();
    renderer.flushAndWait();
    const auto immediateImage = renderer.readbackToImageF32();

    renderer.clear();
    drawBase(true);
    const quint64 workerSubmitCountBefore =
        renderer.deferredWorkerSubmitCount();
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the worker should take ownership of the pending gizmo geometry";
    drawOverlay();
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);

    const auto deferredImage = renderer.readbackToImageF32();
    ASSERT_EQ(deferredImage.width(), immediateImage.width());
    ASSERT_EQ(deferredImage.height(), immediateImage.height());
    float maxChannelError = 0.0f;
    for (int y = 0; y < deferredImage.height(); ++y) {
        for (int x = 0; x < deferredImage.width(); ++x) {
            const auto expected = immediateImage.getPixel(x, y);
            const auto actual = deferredImage.getPixel(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(actual.r() - expected.r()),
                std::abs(actual.g() - expected.g()),
                std::abs(actual.b() - expected.b()),
                std::abs(actual.a() - expected.a())});
        }
    }
    EXPECT_LE(maxChannelError, 1.0e-4f);
    const auto overlap = deferredImage.getPixel(56, 56);
    EXPECT_GT(overlap.b(), 0.8f);
    EXPECT_LT(overlap.r(), 0.6f);
    bool foundGizmoGreen = false;
    for (int y = 0; y < deferredImage.height(); ++y) {
        for (int x = 0; x < deferredImage.width(); ++x) {
            const auto pixel = deferredImage.getPixel(x, y);
            foundGizmoGreen = foundGizmoGreen ||
                (pixel.g() > 0.6f && pixel.r() < 0.4f && pixel.a() > 0.5f);
        }
    }
    EXPECT_TRUE(foundGizmoGreen);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_async_deferred_gizmo_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererWorkerDeferredRecordsClonerAndSpritePackets)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128, true)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    if (!renderer.clonerGridPlacementGpuReady()) {
        GTEST_SKIP() << "Diligent backend could not create the Cloner GPU pipelines";
    }

    auto device = renderer.device();
    ASSERT_NE(device, nullptr);
    constexpr std::array<std::uint8_t, 4> texel{255, 255, 255, 255};
    Diligent::TextureDesc textureDesc;
    textureDesc.Name = "ArtifactIRenderer deferred sprite contract";
    textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    textureDesc.Usage = Diligent::USAGE_IMMUTABLE;
    textureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData subresource;
    subresource.pData = texel.data();
    subresource.Stride = texel.size();
    Diligent::TextureData initialData;
    initialData.pSubResources = &subresource;
    initialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    device->CreateTexture(textureDesc, &initialData, &texture);
    ASSERT_NE(texture, nullptr);
    auto* spriteSRV = texture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(spriteSRV, nullptr);

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    for (int index = 0; index < 12; ++index) {
        renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                               -40.0f, 2.0f, 2.0f,
                               ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    }
    renderer.drawSprite(8.0f, 80.0f, 24.0f, 24.0f, spriteSRV);
    QMatrix4x4 transform;
    transform.setToIdentity();
    renderer.drawSpriteTransformed(72.0f, 72.0f, 24.0f, 24.0f,
                                   transform, spriteSRV);
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), 0u);

    Artifact::ClonerGridPlacementRange placementRange;
    ASSERT_TRUE(renderer.dispatchClonerGridPlacement(
        2, 1, 1, 40.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.75f,
        placementRange));
    ASSERT_EQ(placementRange.count, 3u);
    for (std::uint32_t index = 0; index < placementRange.count; ++index) {
        ASSERT_TRUE(renderer.drawSpriteTransformedClonerGrid(
            64.0f, 64.0f, 12.0f, 8.0f, transform, transform, spriteSRV,
            1.0f, placementRange, placementRange.firstIndex + index));
    }

    renderer.setCanvasSize(128.0f, 128.0f);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);
    ArtifactCore::ParticleRenderData particleData;
    particleData.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    particleData.options.billboard =
        ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    particleData.particles.emplace_back();
    auto& particle = particleData.particles.back();
    particle.r = 1.0f;
    particle.g = 0.2f;
    particle.b = 0.05f;
    particle.a = 1.0f;
    particle.size = 0.35f;
    particle.lifetime = 1.0f;
    renderer.drawParticles(particleData);

    QMatrix4x4 triangleTransform;
    triangleTransform.setToIdentity();
    triangleTransform.translate(96.0f, 80.0f);
    triangleTransform.scale(24.0f, 24.0f);
    renderer.drawTexturedTriangleTransformed(
        Artifact::Detail::float2(0.0f, 0.0f),
        Artifact::Detail::float2(1.0f, 0.0f),
        Artifact::Detail::float2(0.0f, 1.0f),
        Artifact::Detail::float2(0.0f, 0.0f),
        Artifact::Detail::float2(1.0f, 0.0f),
        Artifact::Detail::float2(0.0f, 1.0f),
        triangleTransform, spriteSRV);
    QImage maskImage(1, 1, QImage::Format_RGBA8888);
    maskImage.fill(Qt::white);
    renderer.drawMaskedTextureLocal(96.0f, 104.0f, 24.0f, 20.0f,
                                    spriteSRV, maskImage);

    ArtifactCore::ArtifactArray<Artifact::MpmSurfaceVertex> mpmVertices;
    appendMaximumMpmTestSurface(mpmVertices);
    ASSERT_TRUE(renderer.drawMpmSurface(
        mpmVertices, ArtifactCore::FloatColor(0.1f, 0.9f, 0.25f, 1.0f)));
    texture = nullptr;

    const quint64 workerSubmitCountBefore =
        renderer.deferredWorkerSubmitCount();
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "ParticleRenderer state should be leased to the worker for this packet batch";
    renderer.drawSolidRect(100.0f, 8.0f, 20.0f, 20.0f,
                           ArtifactCore::FloatColor(0.1f, 0.2f, 0.95f, 1.0f));
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1)
        << "the production ArtifactIRenderer path should record cloner, sprite, particle, and MPM surface packets on its worker";
    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), 128);
    ASSERT_EQ(image.height(), 128);
    EXPECT_GT(image.getPixel(20, 90).a(), 0.8f);
    EXPECT_GT(image.getPixel(20, 90).r(), 0.8f);
    EXPECT_GT(image.getPixel(84, 84).a(), 0.8f);
    EXPECT_GT(image.getPixel(84, 84).r(), 0.8f);
    const auto texturedTrianglePixel = image.getPixel(102, 86);
    EXPECT_GT(texturedTrianglePixel.a(), 0.8f);
    EXPECT_GT(texturedTrianglePixel.r(), 0.8f);
    EXPECT_GT(texturedTrianglePixel.g(), 0.8f);
    EXPECT_GT(texturedTrianglePixel.b(), 0.8f);
    const auto maskedSpritePixel = image.getPixel(102, 110);
    EXPECT_GT(maskedSpritePixel.a(), 0.8f);
    EXPECT_GT(maskedSpritePixel.r(), 0.8f);
    EXPECT_GT(maskedSpritePixel.g(), 0.8f);
    EXPECT_GT(maskedSpritePixel.b(), 0.8f);
    for (const int x : {53, 70, 93}) {
        const auto clonerSpriteCenter = image.getPixel(x, 67);
        EXPECT_GT(clonerSpriteCenter.a(), 0.8f) << "x=" << x;
        EXPECT_GT(clonerSpriteCenter.r(), 0.8f) << "x=" << x;
    }
    const auto particleCenter = image.getPixel(64, 64);
    EXPECT_GT(particleCenter.a(), 0.8f);
    EXPECT_GT(particleCenter.r(), 0.8f);
    EXPECT_LT(particleCenter.g(), 0.5f);
    const auto mpmSurfaceCenter = image.getPixel(22, 18);
    EXPECT_GT(mpmSurfaceCenter.a(), 0.8f);
    EXPECT_GT(mpmSurfaceCenter.g(), 0.75f);
    EXPECT_GT(mpmSurfaceCenter.b(), 0.45f);
    EXPECT_LT(mpmSurfaceCenter.b(), 0.65f);
    const auto overlayCenter = image.getPixel(108, 16);
    EXPECT_GT(overlayCenter.b(), 0.8f);
    EXPECT_LT(overlayCenter.r(), 0.4f);
    for (const int x : {52, 82}) {
        const auto spillTriangleCenter = image.getPixel(x, 18);
        EXPECT_GT(spillTriangleCenter.a(), 0.8f) << "x=" << x;
        EXPECT_GT(spillTriangleCenter.g(), 0.75f) << "x=" << x;
    }

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_async_deferred_cloner_sprites_particle_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     SmokeLowAlphaAccumulationIsRepeatable)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(64, 64))
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    renderer.setCanvasSize(64, 64);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    ArtifactCore::ParticleVertex particle{};
    particle.px = particle.ppx = 32.0f;
    particle.py = particle.ppy = 32.0f;
    particle.g = 0.76f;
    particle.size = 32.0f;
    particle.stretch = 1.0f;
    particle.lifetime = 1.0f;
    for (int count : {1, 128}) {
        for (float alpha : {0.0010f, 0.0015f, 0.0020f, 0.0025f, 0.0030f}) {
            particle.a = alpha;
            data.particles.clear();
            for (int i = 0; i < count; ++i) data.particles.push_back(particle);
            const auto renderCenter = [&]() {
                renderer.clear();
                renderer.drawParticles(data);
                renderer.flushAndWait();
                return renderer.readbackToImageF32().getPixel(32, 32).g();
            };
            const float first = renderCenter();
            const float second = renderCenter();
            EXPECT_NEAR(first, second, 1.0e-6f);
            EXPECT_GE(first, 0.0f);
            EXPECT_LE(first, 1.0f);
            // Unquantized linear source-over reference at the sprite center.
            // This is a mathematical control, not a float-target GPU render.
            const double referenceLinear = 0.76 *
                (1.0 - std::pow(1.0 - static_cast<double>(alpha), count));
            EXPECT_GT(referenceLinear, 0.0);
            GTEST_LOG_(INFO) << "smoke low-alpha accumulation count=" << count
                << " alpha=" << alpha << " green=" << first
                << " linear-reference=" << referenceLinear;
        }
    }
}

TEST(ClonerGpuRenderContractTest,
     SmokeSamplingDensityQualityOnDiligentRenderer)
{
    const bool rectangularRnd = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_RECTANGULAR_RND") != 0;
    const bool verticalRnd = rectangularRnd && qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_VERTICAL_RND") != 0;
    bool gridWidthValid = false;
    bool gridHeightValid = false;
    const int requestedGridWidth = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_GRID_WIDTH_RND", &gridWidthValid);
    const int requestedGridHeight = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_GRID_HEIGHT_RND", &gridHeightValid);
    const int gridWidth = gridWidthValid
        ? std::clamp(requestedGridWidth, 24, 192)
        : rectangularRnd ? (verticalRnd ? 64 : 128) : 96;
    const int gridHeight = gridHeightValid
        ? std::clamp(requestedGridHeight, 24, 144)
        : rectangularRnd ? (verticalRnd ? 128 : 64) : 72;
    constexpr int targetImageWidth = 768;
    const int scale = std::max(1, targetImageWidth / gridWidth);
    const bool productionSettingsRnd = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_PRODUCTION_SETTINGS_RND") != 0;
    bool solverIterationsValid = false;
    const int requestedSolverIterations = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_SOLVER_ITERATIONS_RND", &solverIterationsValid);
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = gridWidth;
    settings.gridHeight = gridHeight;
    settings.solverIterations = solverIterationsValid
        ? std::clamp(requestedSolverIterations, 1, 64)
        : productionSettingsRnd ? 20 : 1;
    settings.emitterX = 0.5f;
    settings.emitterY = productionSettingsRnd ? 0.9f : 0.78f;
    settings.emitterRadius = productionSettingsRnd ? 0.10f : 0.08f;
    settings.emitterDensity = productionSettingsRnd ? 0.16f : 0.8f;
    settings.emitterVelocityY = productionSettingsRnd ? -1.0f : -1.2f;
    settings.vorticity = productionSettingsRnd ? 0.1f : 1.5f;
    settings.emitterSwirl = productionSettingsRnd ? 0.0f : 0.6f;
    bool emitterCountValid = false;
    const int requestedEmitterCount = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_EMITTER_COUNT_RND", &emitterCountValid);
    if (emitterCountValid)
        settings.emitterCount = std::clamp(requestedEmitterCount, 1, 64);
    else if (productionSettingsRnd)
        settings.emitterCount = 7;
    const auto smokeFloatOverride = [](const char* name,
                                       const float fallback) {
        bool valid = false;
        const float value = qEnvironmentVariable(name).toFloat(&valid);
        return valid && std::isfinite(value) ? value : fallback;
    };
    settings.vorticity = smokeFloatOverride(
        "ARTIFACT_SMOKE_VORTICITY_RND", settings.vorticity);
    settings.buoyancy = smokeFloatOverride(
        "ARTIFACT_SMOKE_BUOYANCY_RND", settings.buoyancy);
    settings.emitterSwirl = smokeFloatOverride(
        "ARTIFACT_SMOKE_SWIRL_RND", settings.emitterSwirl);
    settings.emitterVelocityY = smokeFloatOverride(
        "ARTIFACT_SMOKE_EMITTER_VELOCITY_Y_RND", settings.emitterVelocityY);
    settings.emitterRadius = smokeFloatOverride(
        "ARTIFACT_SMOKE_EMITTER_RADIUS_RND", settings.emitterRadius);
    settings.emitterDensity = smokeFloatOverride(
        "ARTIFACT_SMOKE_EMITTER_DENSITY_RND", settings.emitterDensity);
    settings.colorR = 0.12f;
    settings.colorG = 0.76f;
    settings.colorB = 0.28f;
    Artifact::LayerFluidRuntimeState state;
    for (int frame = 0; frame <= 44; ++frame) {
        Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
    }
    ASSERT_NE(state.fluidSolver_, nullptr);
    double densityMass = 0.0;
    double densityMomentY = 0.0;
    double upperThirdDensity = 0.0;
    float peakDensity = 0.0f;
    for (int y = 0; y < gridHeight; ++y)
        for (int x = 0; x < gridWidth; ++x)
            peakDensity = std::max(peakDensity,
                state.fluidSolver_->getDensity(x, y));
    int minActiveY = gridHeight;
    int maxActiveY = -1;
    for (int y = 0; y < gridHeight; ++y)
        for (int x = 0; x < gridWidth; ++x) {
            const double density = std::max(0.0f,
                state.fluidSolver_->getDensity(x, y));
            densityMass += density;
            densityMomentY += density * (y + 0.5);
            if (y < gridHeight / 3) upperThirdDensity += density;
            if (density >= 0.05f * peakDensity) {
                minActiveY = std::min(minActiveY, y);
                maxActiveY = std::max(maxActiveY, y);
            }
        }
    GTEST_LOG_(INFO) << "Diligent smoke solver vertical distribution grid="
        << gridWidth << 'x' << gridHeight
        << " emitterCount=" << settings.emitterCount
        << " baseIterations=" << settings.solverIterations
        << " normalizedMass=" << densityMass /
            (static_cast<double>(gridWidth) * gridHeight)
        << " centroidY01=" << (densityMass > 0.0
            ? densityMomentY / densityMass / gridHeight : 0.0)
        << " upperThirdFraction=" << (densityMass > 0.0
            ? upperThirdDensity / densityMass : 0.0)
        << " peak5pctYBounds=" << minActiveY << ':' << maxActiveY;
    QMatrix4x4 transform;
    transform.setToIdentity();
    const QRectF bounds(0.0, 0.0, gridWidth, gridHeight);
    const auto& productionData = Artifact::prepareLayerSmokeRenderData(
        state, bounds, transform, 1.0f, 44, settings);

    const auto makeStream = [&](const Artifact::LayerFluidRuntimeState& sampleState,
                                const int countX, const int countY,
                                const float minimumDensity = 0.025f,
                                const bool smoothLowDensityAlpha = false,
                                const bool linearDensityAlpha = false,
                                const float velocityFollow = 0.0f,
                                std::vector<std::uint8_t>* activeMask = nullptr) {
        ArtifactCore::ParticleRenderData data;
        data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
        data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
        if (activeMask)
            activeMask->assign(
                static_cast<std::size_t>(countX * countY), 0u);
        const float stepX = static_cast<float>(gridWidth - 1) / (countX - 1);
        const float stepY = static_cast<float>(gridHeight - 1) / (countY - 1);
        const float spacing = std::max(
            static_cast<float>(gridWidth) / (countX - 1),
            static_cast<float>(gridHeight) / (countY - 1));
        data.particles.reserve(static_cast<std::size_t>(countX * countY));
        for (int sy = 0; sy < countY; ++sy) {
            for (int sx = 0; sx < countX; ++sx) {
                std::uint32_t hash = static_cast<std::uint32_t>(sx) * 0x9E3779B9u ^
                    static_cast<std::uint32_t>(sy) * 0x85EBCA6Bu;
                hash ^= hash >> 16u;
                hash *= 0x7FEB352Du;
                hash ^= hash >> 15u;
                const float jitterX = static_cast<float>(hash & 0xFFFFu) *
                    (2.0f / 65535.0f) - 1.0f;
                const float jitterY = static_cast<float>(hash >> 16u) *
                    (2.0f / 65535.0f) - 1.0f;
                float gx = std::clamp(sx * stepX + jitterX * stepX * 0.30f,
                                             0.0f, static_cast<float>(gridWidth - 1));
                float gy = std::clamp(sy * stepY + jitterY * stepY * 0.30f,
                                             0.0f, static_cast<float>(gridHeight - 1));
                if (velocityFollow > 0.0f) {
                    const int vx0 = static_cast<int>(gx);
                    const int vy0 = static_cast<int>(gy);
                    const int vx1 = std::min(vx0 + 1, gridWidth - 1);
                    const int vy1 = std::min(vy0 + 1, gridHeight - 1);
                    const float vtx = gx - vx0;
                    const float vty = gy - vy0;
                    const auto velocityAt = [&](const int x, const int y,
                                                float& vx, float& vy) {
                        sampleState.fluidSolver_->getVelocity(x, y, vx, vy);
                    };
                    float vx00 = 0.0f, vy00 = 0.0f;
                    float vx10 = 0.0f, vy10 = 0.0f;
                    float vx01 = 0.0f, vy01 = 0.0f;
                    float vx11 = 0.0f, vy11 = 0.0f;
                    velocityAt(vx0, vy0, vx00, vy00);
                    velocityAt(vx1, vy0, vx10, vy10);
                    velocityAt(vx0, vy1, vx01, vy01);
                    velocityAt(vx1, vy1, vx11, vy11);
                    const float vx = std::lerp(std::lerp(vx00, vx10, vtx),
                                               std::lerp(vx01, vx11, vtx), vty);
                    const float vy = std::lerp(std::lerp(vy00, vy10, vtx),
                                               std::lerp(vy01, vy11, vtx), vty);
                    gx = std::clamp(gx - vx * velocityFollow,
                                    0.0f, static_cast<float>(gridWidth - 1));
                    gy = std::clamp(gy - vy * velocityFollow,
                                    0.0f, static_cast<float>(gridHeight - 1));
                }
                const int x0 = static_cast<int>(gx);
                const int y0 = static_cast<int>(gy);
                const int x1 = std::min(x0 + 1, gridWidth - 1);
                const int y1 = std::min(y0 + 1, gridHeight - 1);
                const float tx = gx - x0;
                const float ty = gy - y0;
                const auto densityAt = [&](const int x, const int y) {
                    return sampleState.fluidSolver_->getDensity(x, y);
                };
                const float density = std::lerp(
                    std::lerp(densityAt(x0, y0), densityAt(x1, y0), tx),
                    std::lerp(densityAt(x0, y1), densityAt(x1, y1), tx), ty);
                if ((minimumDensity > 0.0f && density < minimumDensity) ||
                    (smoothLowDensityAlpha && density <= 1.0e-6f)) continue;
                ArtifactCore::ParticleVertex p{};
                p.px = gx * scale;
                p.py = gy * scale;
                p.ppx = p.px;
                p.ppy = p.py;
                p.r = 0.12f;
                p.g = 0.76f;
                p.b = 0.28f;
                p.a = linearDensityAlpha
                    ? std::clamp(density * 0.36f, 0.0f, 0.18f) * 0.79f
                    : smoothLowDensityAlpha
                        ? std::clamp(std::sqrt(density) * 0.18f, 0.0f, 0.18f) * 0.79f
                        : std::clamp(std::sqrt(density) * 0.18f, 0.01f, 0.18f) * 0.79f;
                p.size = spacing * scale * (0.118f + density * 0.02f) * 1.125f;
                data.particles.push_back(p);
                if (activeMask)
                    (*activeMask)[static_cast<std::size_t>(
                        sy * countX + sx)] = 1u;
            }
        }
        return data;
    };
    struct Variant {
        const char* name;
        ArtifactCore::ParticleRenderData data;
        double alphaScale = 1.0;
        ArtifactCore::ImageF32x4_RGBA image;
    };
    std::array<Variant, 13> variants{{
        {"current-40x40", makeStream(state, 40, 40)},
        {"resample-64x48", makeStream(state, 64, 48)},
        {"resample-96x72", makeStream(state, 96, 72)},
        {"soft-multikernel-40x40", makeStream(state, 40, 40)},
        {"gaussian-density-40x40", makeStream(state, 40, 40)},
        {"gaussian-density-tight-40x40", makeStream(state, 40, 40)},
        {"gaussian-density-wide-40x40", makeStream(state, 40, 40)},
        {rectangularRnd ? "production-gaussian-64x64" : "production-gaussian-64x48", productionData},
        {"gaussian-density-96x72", makeStream(state, 96, 72)},
        {"gaussian-density-64x32", makeStream(state, 64, 32)},
        {"gaussian-density-128x64", makeStream(state, 128, 64)},
        {"gaussian-wide-128x64", makeStream(state, 128, 64)},
        {"native-resolution", makeStream(state, gridWidth, gridHeight)},
    }};
    for (std::size_t index = 0; index < variants.size(); ++index) {
        if (index != 7) continue;
        for (auto& particle : variants[index].data.particles) {
            particle.px *= static_cast<float>(scale);
            particle.py *= static_cast<float>(scale);
            particle.ppx *= static_cast<float>(scale);
            particle.ppy *= static_cast<float>(scale);
            particle.size *= static_cast<float>(scale);
        }
    }
    {
        auto& layered = variants[3].data;
        layered.particles.clear();
        layered.particles.reserve(productionData.particles.size() * 3);
        for (const auto& source : variants[0].data.particles) {
            auto outer = source;
            outer.size *= 1.65f;
            outer.a *= 0.045f;
            layered.particles.push_back(outer);

            auto middle = source;
            middle.size *= 1.20f;
            middle.a *= 0.075f;
            layered.particles.push_back(middle);

            auto inner = source;
            inner.size *= 0.72f;
            inner.a *= 0.12f;
            layered.particles.push_back(inner);
        }
    }
    variants[4].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[5].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[6].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[7].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[8].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[9].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[10].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[11].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    variants[12].data.options.falloff =
        ArtifactCore::ParticleFalloffMode::GaussianDensity;
    for (auto& particle : variants[5].data.particles)
        particle.size *= 0.78f;
    for (auto& particle : variants[6].data.particles)
        particle.size *= 1.18f;
    for (auto& particle : variants[11].data.particles)
        particle.size *= 1.5f;

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(gridWidth * scale, gridHeight * scale))
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    renderer.setCanvasSize(gridWidth * scale, gridHeight * scale);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    const auto render = [&](const ArtifactCore::ParticleRenderData& data) {
        renderer.clear();
        renderer.drawParticles(data);
        renderer.flushAndWait();
        return renderer.readbackToImageF32();
    };
    variants[0].image = render(variants[0].data);
    const auto exposure = [](const ArtifactCore::ImageF32x4_RGBA& image) {
        double sum = 0.0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                sum += image.getPixel(x, y).g();
        return sum;
    };
    const double targetExposure = exposure(variants[0].image);
    ASSERT_GT(targetExposure, 1.0)
        << "the production smoke fixture must produce visible GPU pixels";
    for (std::size_t i = 1; i < variants.size(); ++i) {
        float low = 0.0f;
        float high = 8.0f;
        for (int iteration = 0; iteration < 14; ++iteration) {
            const float mid = (low + high) * 0.5f;
            auto candidate = variants[i].data;
            for (auto& p : candidate.particles) p.a *= mid;
            if (exposure(render(candidate)) < targetExposure) low = mid;
            else high = mid;
        }
        variants[i].alphaScale = (low + high) * 0.5f;
        for (auto& p : variants[i].data.particles)
            p.a *= static_cast<float>(variants[i].alphaScale);
        variants[i].image = render(variants[i].data);
    }
    if (!productionSettingsRnd && !rectangularRnd &&
        gridWidth == 96 && gridHeight == 72) {
        EXPECT_NEAR(variants[7].alphaScale, 1.0, 0.05)
            << "the production sampling compensation should preserve exposure";
    }
    const auto imageQuality = [](const ArtifactCore::ImageF32x4_RGBA& image) {
        constexpr int tileSize = 8;
        const int tileColumns = image.width() / tileSize;
        const int tileRows = image.height() / tileSize;
        std::vector<double> tileMeans(static_cast<std::size_t>(
            tileColumns * tileRows), 0.0);
        double highFrequencySquared = 0.0;
        double energy = 0.0;
        std::size_t pixelCount = 0;
        std::size_t highFrequencyCount = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const double center = image.getPixel(x, y).g();
                energy += center;
                ++pixelCount;
                const auto tile = static_cast<std::size_t>(
                    (y / tileSize) * tileColumns + x / tileSize);
                tileMeans[tile] += center;
                if (x > 0 && y > 0 && x + 1 < image.width() &&
                    y + 1 < image.height()) {
                    const double neighbors = 0.25 * (
                        image.getPixel(x - 1, y).g() +
                        image.getPixel(x + 1, y).g() +
                        image.getPixel(x, y - 1).g() +
                        image.getPixel(x, y + 1).g());
                    const double residual = center - neighbors;
                    highFrequencySquared += residual * residual;
                    ++highFrequencyCount;
                }
            }
        }
        for (auto& mean : tileMeans)
            mean /= static_cast<double>(tileSize * tileSize);
        std::size_t activeTiles = 0;
        std::size_t activeTileEdges = 0;
        for (int y = 0; y < tileRows; ++y)
            for (int x = 0; x < tileColumns; ++x) {
                const auto tile = static_cast<std::size_t>(y * tileColumns + x);
                if (tileMeans[tile] < 0.01) continue;
                ++activeTiles;
                if (x + 1 < tileColumns &&
                    tileMeans[tile + 1] >= 0.01)
                    ++activeTileEdges;
                if (y + 1 < tileRows &&
                    tileMeans[tile + tileColumns] >= 0.01)
                    ++activeTileEdges;
            }
        const double componentCount = static_cast<double>(
            findVisibleComponents(image).size());
        const double normalizedBoundary = activeTiles > 0
            ? static_cast<double>(activeTileEdges) /
                static_cast<double>(activeTiles) : 0.0;
        return std::array<double, 4>{
            highFrequencyCount > 0
                ? std::sqrt(highFrequencySquared / highFrequencyCount) : 0.0,
            activeTiles > 0
                ? static_cast<double>(activeTileEdges) /
                    static_cast<double>(activeTiles) : 0.0,
            componentCount,
            energy > 0.0 && pixelCount > 0
                ? std::sqrt(highFrequencySquared / highFrequencyCount) /
                    std::sqrt(energy / static_cast<double>(pixelCount)) : 0.0};
    };
    const auto meanGpuSubmitMs = [&](const ArtifactCore::ParticleRenderData& data) {
        constexpr int warmup = 3;
        constexpr int measured = 12;
        for (int i = 0; i < warmup; ++i) {
            renderer.clear();
            renderer.drawParticles(data);
            renderer.flushAndWait();
        }
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < measured; ++i) {
            renderer.clear();
            renderer.drawParticles(data);
            renderer.flushAndWait();
        }
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count() /
            static_cast<double>(measured);
    };
    const QString output = qEnvironmentVariable("ARTIFACT_SMOKE_GPU_OUTPUT_DIR");
    for (const auto& variant : variants) {
        const auto quality = imageQuality(variant.image);
        GTEST_LOG_(INFO) << "Diligent smoke sampling " << variant.name
            << " particles=" << variant.data.particles.size()
            << " alphaScale=" << variant.alphaScale
            << " exposure=" << exposure(variant.image)
            << " hf-rms/normalizedHf/activeComponents/tileBoundary="
            << quality[0] << '/' << quality[3] << '/' << quality[2] << '/'
            << quality[1]
            << " mean-gpu-submit-ms=" << meanGpuSubmitMs(variant.data);
        EXPECT_NEAR(exposure(variant.image), targetExposure, targetExposure * 0.01);
        if (!output.isEmpty()) {
            ASSERT_TRUE(QDir().mkpath(output));
            EXPECT_TRUE(variant.image.toQImage().save(
                QDir(output).filePath(QStringLiteral("%1.png").arg(
                    QString::fromLatin1(variant.name))), "PNG"));
        }
    }

    if (!output.isEmpty()) {
        const bool fixedTemporalExposure = qEnvironmentVariableIntValue(
            "ARTIFACT_SMOKE_FIXED_TEMPORAL_EXPOSURE") == 1;
        const bool adjacentTemporalFrames = qEnvironmentVariableIntValue(
            "ARTIFACT_SMOKE_ADJACENT_TEMPORAL_FRAMES") == 1;
        const std::array<int, 3> temporalFrames = adjacentTemporalFrames
            ? std::array<int, 3>{42, 43, 44}
            : std::array<int, 3>{20, 32, 44};
        struct TemporalSample final {
            int countX;
            int countY;
            float minimumDensity;
            bool smoothLowDensityAlpha;
            bool linearDensityAlpha;
            float velocityFollow;
            const char* label;
        };
        const std::array<TemporalSample, 9> temporalSamples{{
            {40, 40, 0.025f, false, false, 0.0f, "baseline-40x40"},
            {64, 48, 0.025f, false, false, 0.0f,
                rectangularRnd ? "resample-64x48" : "production-64x48"},
            {64, 48, 0.005f, true, false, 0.0f, "soft-tail-64x48"},
            {64, 48, 0.0f, true, false, 0.0f, "sqrt-alpha-no-cutoff-64x48"},
            {64, 48, 0.0f, true, true, 0.0f, "linear-alpha-no-cutoff-64x48"},
            {64, 48, 0.025f, false, false, 1.0f, "velocity-follow-1x-64x48"},
            {96, 72, 0.025f, false, false, 0.0f, "resample-96x72"},
            {64, 64, 0.025f, false, false, 0.0f, "resample-64x64"},
            {verticalRnd ? 64 : 128, verticalRnd ? 128 : 64, 0.025f,
                false, false, 0.0f, verticalRnd ? "wide-64x128" : "wide-128x64"}}};
        Artifact::LayerFluidRuntimeState temporalState;
        std::array<std::vector<std::uint8_t>, 9> previousMasks;
        std::array<double, 9> fixedAlphaScale{};
        for (int frame = 0; frame <= temporalFrames.back(); ++frame) {
            Artifact::advanceLayerSmokeRuntime(
                temporalState, frame, 30.0, settings);
            if (frame != temporalFrames[0] &&
                frame != temporalFrames[1] &&
                frame != temporalFrames[2]) continue;
            std::array<ArtifactCore::ImageF32x4_RGBA, 9> temporalImages;
            std::array<double, 9> temporalAlphaScale{};
            std::array<std::size_t, 9> birthCounts{};
            std::array<std::size_t, 9> deathCounts{};
            std::array<double, 9> persistence{};
            for (std::size_t sample = 0; sample < temporalSamples.size();
                 ++sample) {
                std::vector<std::uint8_t> activeMask;
                auto data = makeStream(temporalState,
                    temporalSamples[sample].countX,
                    temporalSamples[sample].countY,
                    temporalSamples[sample].minimumDensity,
                    temporalSamples[sample].smoothLowDensityAlpha,
                    temporalSamples[sample].linearDensityAlpha,
                    temporalSamples[sample].velocityFollow,
                    &activeMask);
                data.options.falloff =
                    ArtifactCore::ParticleFalloffMode::GaussianDensity;
                if (fixedTemporalExposure && sample == 0) {
                    // Compare fixed-exposure candidates against the actual runtime
                    // extraction, including its alpha compensation and bounds mapping.
                    data = Artifact::prepareLayerSmokeRenderData(
                        temporalState, bounds, transform, 1.0f, frame, settings);
                    for (auto& particle : data.particles) {
                        particle.px *= scale;
                        particle.py *= scale;
                        particle.ppx *= scale;
                        particle.ppy *= scale;
                        particle.size *= scale;
                    }
                    // The test-only 40x40 mask does not describe production samples.
                    activeMask.clear();
                }
                if (sample == 8) {
                    for (auto& particle : data.particles) particle.size *= 1.5f;
                }
                temporalAlphaScale[sample] = 1.0;
                if (sample == 0) {
                    temporalImages[sample] = render(data);
                    ASSERT_GT(exposure(temporalImages[sample]), 1.0)
                        << "temporal smoke frame must produce visible GPU pixels";
                } else {
                    const double targetFrameExposure =
                        exposure(temporalImages[0]);
                    ASSERT_GT(targetFrameExposure, 1.0);
                    float low = 0.0f;
                    float high = 8.0f;
                    if (!fixedTemporalExposure || frame == temporalFrames[0]) {
                        for (int iteration = 0; iteration < 14; ++iteration) {
                            const float mid = (low + high) * 0.5f;
                            auto candidate = data;
                            for (auto& particle : candidate.particles)
                                particle.a *= mid;
                            if (exposure(render(candidate)) < targetFrameExposure)
                                low = mid;
                            else
                                high = mid;
                        }
                        fixedAlphaScale[sample] = (low + high) * 0.5f;
                    }
                    temporalAlphaScale[sample] = fixedAlphaScale[sample];
                    for (auto& particle : data.particles)
                        particle.a *= static_cast<float>(
                            temporalAlphaScale[sample]);
                    temporalImages[sample] = render(data);
                    if (!fixedTemporalExposure || frame == temporalFrames[0]) {
                        EXPECT_NEAR(exposure(temporalImages[sample]),
                            targetFrameExposure, targetFrameExposure * 0.01);
                    }
                }
                const auto& previousMask = previousMasks[sample];
                if (!activeMask.empty() && previousMask.size() == activeMask.size()) {
                    std::size_t intersection = 0;
                    std::size_t unionCount = 0;
                    for (std::size_t index = 0; index < activeMask.size(); ++index) {
                        const bool wasActive = previousMask[index] != 0u;
                        const bool isActive = activeMask[index] != 0u;
                        if (wasActive && isActive) ++intersection;
                        if (wasActive || isActive) ++unionCount;
                        if (!wasActive && isActive) ++birthCounts[sample];
                        if (wasActive && !isActive) ++deathCounts[sample];
                    }
                    persistence[sample] = unionCount > 0
                        ? static_cast<double>(intersection) / unionCount : 1.0;
                }
                previousMasks[sample] = std::move(activeMask);
                const auto quality = imageQuality(temporalImages[sample]);
                GTEST_LOG_(INFO) << "Diligent smoke temporal frame=" << frame
                    << " sampling=" << (fixedTemporalExposure && sample == 0
                        ? "actual-production" : temporalSamples[sample].label)
                    << " particles=" << data.particles.size()
                    << " alphaScale=" << temporalAlphaScale[sample]
                    << " exposure=" << exposure(temporalImages[sample])
                    << " normalizedHf/components/tileBoundary="
                    << quality[3] << '/' << quality[2] << '/' << quality[1]
                    << " activeBirths/deaths/jaccard="
                    << birthCounts[sample] << '/' << deathCounts[sample] << '/'
                    << persistence[sample];
                const auto captureName = QStringLiteral(
                    "temporal-frame-%1-sampling-%2.png")
                    .arg(frame, 2, 10, QLatin1Char('0'))
                    .arg(QString::fromLatin1(fixedTemporalExposure && sample == 0
                        ? "actual-production" : temporalSamples[sample].label));
                EXPECT_TRUE(temporalImages[sample].toQImage().save(
                    QDir(output).filePath(captureName), "PNG"));
            }
        }
    }
}

TEST(ClonerGpuRenderContractTest,
     MeasuresProductionSmokeSourceAndTopOutflowOnDiligentRenderer)
{
    const auto readDimension = [](const char* name, const int fallback,
                                  const int minimum, const int maximum) {
        bool valid = false;
        const int value = qEnvironmentVariableIntValue(name, &valid);
        return valid ? std::clamp(value, minimum, maximum) : fallback;
    };
    const int outputWidth = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_OUTPUT_WIDTH_RND", 768, 128, 1024);
    const int outputHeight = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_OUTPUT_HEIGHT_RND", 432, 128, 1024);
    const int frame = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_FRAME_RND", 60, 0, 240);
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputWidth, outputHeight)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    QMatrix4x4 identity;
    identity.setToIdentity();
    struct Condition {
        const char* label;
        float emitterRadius;
        float emitterDensity;
        float outflowBand;
        float densityDissipationPerSecond;
        float smokeTurbulence;
        int smokeTurbulenceOctaves;
        float emitterVelocityY;
        float emitterTemperature;
        bool highOrderDensityAdvection;
        int gridWidth;
        int gridHeight;
    };
    constexpr std::array<Condition, 16> conditions{{
        {"narrow-r016-d0625-band30", 0.16f, 0.0625f, 0.30f, 2.5f, 0.0f, 1, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-band30", 0.20f, 0.04f, 0.30f, 2.5f, 0.0f, 1, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-band35", 0.20f, 0.04f, 0.35f, 2.5f, 0.0f, 1, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-diss050", 0.20f, 0.04f, 0.30f, 0.5f, 0.0f, 1, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-curl050-oct1", 0.20f, 0.04f, 0.30f, 2.5f, 0.5f, 1, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-band35-curl050-oct2", 0.20f, 0.04f, 0.35f, 2.5f, 0.5f, 2, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-curl100-oct1", 0.20f, 0.04f, 0.30f, 2.5f, 1.0f, 1, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-curl100-oct2", 0.20f, 0.04f, 0.30f, 2.5f, 1.0f, 2, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-curl100-oct3", 0.20f, 0.04f, 0.30f, 2.5f, 1.0f, 3, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-diss050-curl100-oct2", 0.20f, 0.04f, 0.30f, 0.5f, 1.0f, 2, -0.75f, 1.0f, false, 128, 128},
        {"wide-r020-d004-smoke-slow-rise", 0.20f, 0.04f, 0.30f, 2.5f, 0.0f, 1, -0.40f, 0.0f, false, 128, 128},
        {"wide-r020-d004-smoke-slow-rise-diss050", 0.20f, 0.04f, 0.30f, 0.5f, 0.0f, 1, -0.40f, 0.0f, false, 128, 128},
        {"wide-r020-d004-high-order-advection", 0.20f, 0.04f, 0.30f, 2.5f, 0.0f, 1, -0.75f, 1.0f, true, 128, 128},
        {"wide-r020-d004-grid384-high-order-off", 0.20f, 0.04f, 0.30f, 2.5f, 0.0f, 1, -0.75f, 1.0f, false, 384, 216},
        {"wide-r020-d004-grid384-high-order-on", 0.20f, 0.04f, 0.30f, 2.5f, 0.0f, 1, -0.75f, 1.0f, true, 384, 216},
        {"wide-r020-d004-grid192-current-default", 0.20f, 0.04f, 0.35f, 2.5f, 0.0f, 1, -0.75f, 1.0f, false, 192, 192},
    }};
    const QString requestedOutputDirectory = qEnvironmentVariable(
        "ARTIFACT_SMOKE_PRODUCTION_OUTPUT_DIR_RND");
    const QString outputDirectory = requestedOutputDirectory.isEmpty()
        ? QDir::tempPath() : requestedOutputDirectory;
    ASSERT_TRUE(QDir().mkpath(outputDirectory));
    for (const auto& condition : conditions) {
        renderer.clear();
        Artifact::LayerSmokeRuntimeSettings settings;
        settings.gridWidth = condition.gridWidth;
        settings.gridHeight = condition.gridHeight;
        settings.solverAdaptiveIterations = false;
        settings.emitterX = 0.5f;
        settings.emitterY = 0.90f;
        settings.emitterRadius = condition.emitterRadius;
        settings.emitterCount = 7;
        settings.emitterDensity = condition.emitterDensity;
        settings.emitterVelocityY = condition.emitterVelocityY;
        settings.emitterTemperature = condition.emitterTemperature;
        settings.thermalBuoyancy = 0.0f;
        settings.vorticity = 0.1f;
        settings.buoyancy = 0.0f;
        settings.emitterSwirl = 0.0f;
        settings.densityDissipationPerSecond =
            condition.densityDissipationPerSecond;
        settings.volumeDetailStrength = 0.9f;
        settings.smokeTurbulence = condition.smokeTurbulence;
        settings.smokeTurbulenceScale = 18.0f;
        settings.smokeTurbulenceOctaves =
            condition.smokeTurbulenceOctaves;
        settings.highOrderDensityAdvection =
            condition.highOrderDensityAdvection;
        settings.topOutflowRatePerSecond = 100.0f;
        settings.topOutflowBandFraction = condition.outflowBand;
        settings.colorR = 0.42f;
        settings.colorG = 0.72f;
        settings.colorB = 1.0f;
        Artifact::LayerFluidRuntimeState runtime;
        const auto simulationStart = std::chrono::steady_clock::now();
        Artifact::advanceLayerSmokeRuntime(runtime, frame, 30.0, settings);
        const double simulationMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - simulationStart).count();
        const auto renderStart = std::chrono::steady_clock::now();
        Artifact::renderLayerSmokeRuntime(runtime, &renderer,
            QRectF(0.0, 0.0, outputWidth, outputHeight), identity, 1.0f,
            frame, 30.0, settings);
        ASSERT_NE(runtime.fluidSolver_, nullptr);
        renderer.flushAndWait();
        const auto image = renderer.readbackToImageF32();
        const double renderFlushReadbackMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - renderStart).count();
        ASSERT_EQ(image.width(), outputWidth);
        ASSERT_EQ(image.height(), outputHeight);
        double densityMass = 0.0;
        std::array<double, 3> densityThirdMass{};
        const int gridWidth = runtime.fluidSolver_->width();
        const int gridHeight = runtime.fluidSolver_->height();
        for (int y = 1; y < gridHeight - 1; ++y) {
            for (int x = 1; x < gridWidth - 1; ++x) {
                const double density = runtime.fluidSolver_->getDensity(x, y);
                densityMass += density;
                const auto third = std::min(2, 3 * y / gridHeight);
                densityThirdMass[static_cast<std::size_t>(third)] += density;
            }
        }
        double lumaMass = 0.0;
        double lumaMomentY = 0.0;
        std::size_t warmPixels = 0;
        double warmExcess = 0.0;
        std::array<double, 3> lumaThirdMass{};
        std::array<std::size_t, 3> visibleThirdPixels{};
        std::size_t visiblePixels = 0;
        int visibleTop = outputHeight;
        int visibleBottom = -1;
        for (int y = 0; y < outputHeight; ++y) {
            for (int x = 0; x < outputWidth; ++x) {
                const auto pixel = image.getPixel(x, y);
                const double luma = 0.2126 * pixel.r() +
                    0.7152 * pixel.g() + 0.0722 * pixel.b();
                const double warmContrast = pixel.r() - pixel.b();
                if (warmContrast > 0.05 && pixel.r() > 0.1f) {
                    ++warmPixels;
                    warmExcess += warmContrast;
                }
                lumaMass += luma;
                lumaMomentY += luma * y;
                const auto third = std::min(2, 3 * y / outputHeight);
                lumaThirdMass[static_cast<std::size_t>(third)] += luma;
                if (luma > 0.02) {
                    ++visiblePixels;
                    ++visibleThirdPixels[static_cast<std::size_t>(third)];
                    visibleTop = std::min(visibleTop, y);
                    visibleBottom = std::max(visibleBottom, y);
                }
            }
        }
        double lumaLaplacianSquared = 0.0;
        std::size_t lumaLaplacianSamples = 0;
        for (int y = 1; y + 1 < outputHeight; ++y) {
            for (int x = 1; x + 1 < outputWidth; ++x) {
                const auto lumaAt = [&](const int px, const int py) {
                    const auto pixel = image.getPixel(px, py);
                    return 0.2126 * pixel.r() + 0.7152 * pixel.g() +
                           0.0722 * pixel.b();
                };
                const double center = lumaAt(x, y);
                const double laplacian = lumaAt(x - 1, y) +
                    lumaAt(x + 1, y) + lumaAt(x, y - 1) +
                    lumaAt(x, y + 1) - 4.0 * center;
                lumaLaplacianSquared += laplacian * laplacian;
                ++lumaLaplacianSamples;
            }
        }
        const double highFrequencyLumaRms = std::sqrt(
            lumaLaplacianSquared /
            static_cast<double>(std::max<std::size_t>(
                1, lumaLaplacianSamples)));
        GTEST_LOG_(INFO) << "Production smoke Diligent source/outflow="
            << condition.label << " fieldMass=" << densityMass
            << " fieldThirdShares(top/middle/bottom)="
            << (densityMass > 0.0 ? densityThirdMass[0] / densityMass : 0.0)
            << '/' << (densityMass > 0.0 ? densityThirdMass[1] / densityMass : 0.0)
            << '/' << (densityMass > 0.0 ? densityThirdMass[2] / densityMass : 0.0)
            << " renderedLumaCentroidY="
            << (lumaMass > 0.0 ? lumaMomentY / lumaMass / outputHeight : 0.0)
            << " warmPixels/excess=" << warmPixels << '/' << warmExcess
            << " renderedLumaThirdShares(top/middle/bottom)="
            << (lumaMass > 0.0 ? lumaThirdMass[0] / lumaMass : 0.0) << '/'
            << (lumaMass > 0.0 ? lumaThirdMass[1] / lumaMass : 0.0) << '/'
            << (lumaMass > 0.0 ? lumaThirdMass[2] / lumaMass : 0.0)
            << " visiblePixels/thirds=" << visiblePixels << '/'
            << visibleThirdPixels[0] << '/' << visibleThirdPixels[1] << '/'
            << visibleThirdPixels[2] << " visibleY=" << visibleTop << '-'
            << visibleBottom << " sourceRadius/density/band="
            << condition.emitterRadius << '/' << condition.emitterDensity << '/'
            << condition.outflowBand << " topOutflowRate="
            << settings.topOutflowRatePerSecond << " densityDissipation="
            << condition.densityDissipationPerSecond
            << " emitterVelocityY/temperature="
            << condition.emitterVelocityY << '/'
            << condition.emitterTemperature
            << " highOrderDensityAdvection="
            << condition.highOrderDensityAdvection
            << " grid=" << gridWidth << 'x' << gridHeight
            << " detail/curl/curlScale="
            << settings.volumeDetailStrength << '/'
            << settings.smokeTurbulence << '/'
            << settings.smokeTurbulenceOctaves << '/'
            << settings.smokeTurbulenceScale
            << " highFrequencyLumaRms=" << highFrequencyLumaRms
            << " simulationMs/simulationFrameMs="
            << simulationMilliseconds << '/'
            << simulationMilliseconds / static_cast<double>(
                std::max(1, frame + 1))
            << " renderFlushReadbackMs="
            << renderFlushReadbackMilliseconds;
        const QString capturePath = QDir(outputDirectory).filePath(
            QStringLiteral("production-smoke-gpu-velocity-outflow-%1-frame-%2-%3x%4.png")
                .arg(QString::fromLatin1(condition.label))
                .arg(frame).arg(outputWidth).arg(outputHeight));
        EXPECT_TRUE(image.toQImage().save(capturePath, "PNG"));
        GTEST_LOG_(INFO) << "Production smoke Diligent capture="
            << capturePath.toStdString();
        EXPECT_GT(visiblePixels, 100u);
        EXPECT_GT(visibleThirdPixels[1] + visibleThirdPixels[2], 0u);
    }
}

TEST(ClonerGpuRenderContractTest,
     ProductionSmokeVolumeDrawsSmokeAndFlameFieldsOnDiligentRenderer)
{
    const auto readDimension = [](const char* name, const int fallback,
                                  const int minimum, const int maximum) {
        bool valid = false;
        const int value = qEnvironmentVariableIntValue(name, &valid);
        return valid ? std::clamp(value, minimum, maximum) : fallback;
    };
    const int outputWidth = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_OUTPUT_WIDTH_RND", 768, 128, 1024);
    const int outputHeight = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_OUTPUT_HEIGHT_RND", 432, 128, 1024);
    const int gridWidth = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_GRID_WIDTH_RND", 128, 8, 512);
    const int gridHeight = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_GRID_HEIGHT_RND", 128, 8, 512);
    const int frame = readDimension(
        "ARTIFACT_SMOKE_PRODUCTION_FRAME_RND", 95, 0, 240);
    bool topOutflowRateValid = false;
    const float topOutflowRate = qEnvironmentVariable(
        "ARTIFACT_SMOKE_PRODUCTION_TOP_OUTFLOW_RATE_RND")
        .toFloat(&topOutflowRateValid);
    const float safeTopOutflowRate = topOutflowRateValid &&
        std::isfinite(topOutflowRate)
        ? std::clamp(topOutflowRate, 0.0f, 100.0f) : 60.0f;
    bool topOutflowBandValid = false;
    const float topOutflowBand = qEnvironmentVariable(
        "ARTIFACT_SMOKE_PRODUCTION_TOP_OUTFLOW_BAND_RND")
        .toFloat(&topOutflowBandValid);
    const float safeTopOutflowBand = topOutflowBandValid &&
        std::isfinite(topOutflowBand)
        ? std::clamp(topOutflowBand, 0.01f, 0.5f) : 0.3f;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputWidth, outputHeight)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    QMatrix4x4 identity;
    identity.setToIdentity();
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = gridWidth;
    settings.gridHeight = gridHeight;
    settings.emitterX = 0.5f;
    const auto smokeFloatOverride = [](const char* name, const float fallback) {
        bool valid = false;
        const float value = qEnvironmentVariable(name).toFloat(&valid);
        return valid && std::isfinite(value) ? value : fallback;
    };
    bool emitterCountValid = false;
    const int requestedEmitterCount = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_PRODUCTION_EMITTER_COUNT_RND", &emitterCountValid);
    settings.emitterY = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_EMITTER_Y_RND", 0.9f);
    settings.emitterRadius = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_EMITTER_RADIUS_RND", 0.20f);
    settings.emitterCount = emitterCountValid
        ? std::clamp(requestedEmitterCount, 1, 64) : 7;
    settings.emitterDensity = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_EMITTER_DENSITY_RND", 0.04f);
    settings.emitterVelocityY = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_EMITTER_VELOCITY_Y_RND", -0.75f);
    settings.vorticity = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_VORTICITY_RND", settings.vorticity);
    settings.buoyancy = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_BUOYANCY_RND", settings.buoyancy);
    settings.emitterSwirl = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_SWIRL_RND", settings.emitterSwirl);
    settings.smokeTurbulence = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_CURL_STRENGTH_RND",
        settings.smokeTurbulence);
    bool curlOctavesValid = false;
    const int curlOctaves = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_PRODUCTION_CURL_OCTAVES_RND", &curlOctavesValid);
    if (curlOctavesValid)
        settings.smokeTurbulenceOctaves = std::clamp(curlOctaves, 1, 4);
    settings.volumeDetailStrength = 0.0f;
    settings.emitterTemperature = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_EMITTER_TEMPERATURE_RND", 1.0f);
    settings.thermalBuoyancy = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_THERMAL_BUOYANCY_RND", 0.0f);
    settings.flameLifetimeSeconds = smokeFloatOverride(
        "ARTIFACT_SMOKE_PRODUCTION_FLAME_LIFETIME_RND", 0.65f);
    settings.topOutflowRatePerSecond = safeTopOutflowRate;
    settings.topOutflowBandFraction = safeTopOutflowBand;
    settings.colorR = 0.42f;
    settings.colorG = 0.72f;
    settings.colorB = 1.0f;
    Artifact::LayerFluidRuntimeState runtime;
    const auto renderStart = std::chrono::steady_clock::now();
    Artifact::renderLayerSmokeRuntime(
        runtime, &renderer, QRectF(0.0, 0.0, outputWidth, outputHeight),
        identity, 1.0f, frame, 30.0, settings);
    ASSERT_NE(runtime.fluidSolver_, nullptr);
    ASSERT_TRUE(runtime.flameLifetimeActive_);
    renderer.flushAndWait();

    const auto image = renderer.readbackToImageF32();
    const double renderMilliseconds = std::chrono::duration<double,
        std::milli>(std::chrono::steady_clock::now() - renderStart).count();
    ASSERT_EQ(image.width(), outputWidth);
    ASSERT_EQ(image.height(), outputHeight);
    std::size_t visiblePixels = 0;
    std::size_t warmPixels = 0;
    float maximumRed = 0.0f;
    float maximumGreen = 0.0f;
    float maximumBlue = 0.0f;
    float maximumWarmContrast = 0.0f;
    double lumaMass = 0.0;
    double lumaMomentY = 0.0;
    std::array<std::size_t, 3> lumaThirdPixels{};
    double outputAlphaMass = 0.0;
    double outputAlphaMomentY = 0.0;
    std::array<double, 3> outputAlphaThirdMass{};
    double densityMass = 0.0;
    double densityMomentY = 0.0;
    std::array<double, 3> densityThirdMass{};
    for (int y = 1; y < gridHeight - 1; ++y) {
        for (int x = 1; x < gridWidth - 1; ++x) {
            const double density = runtime.fluidSolver_->getDensity(x, y);
            densityMass += density;
            densityMomentY += density * y;
            const auto third = std::min(2, 3 * y / gridHeight);
            densityThirdMass[static_cast<std::size_t>(third)] += density;
        }
    }
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.getPixel(x, y);
            const double luma = 0.2126 * pixel.r() +
                0.7152 * pixel.g() + 0.0722 * pixel.b();
            lumaMass += luma;
            lumaMomentY += luma * y;
            const auto third = std::min(2, 3 * y / outputHeight);
            const double outputAlpha = std::clamp(
                static_cast<double>(pixel.a()), 0.0, 1.0);
            outputAlphaMass += outputAlpha;
            outputAlphaMomentY += outputAlpha * y;
            outputAlphaThirdMass[static_cast<std::size_t>(third)] +=
                outputAlpha;
            maximumRed = std::max(maximumRed, pixel.r());
            maximumGreen = std::max(maximumGreen, pixel.g());
            maximumBlue = std::max(maximumBlue, pixel.b());
            maximumWarmContrast = std::max(
                maximumWarmContrast, pixel.r() - pixel.b());
            if (pixel.r() + pixel.g() + pixel.b() > 0.015f) ++visiblePixels;
            if (luma > 0.02)
                ++lumaThirdPixels[static_cast<std::size_t>(third)];
            if (pixel.r() > pixel.g() * 1.05f &&
                pixel.g() > pixel.b() * 1.05f && pixel.r() > 0.02f) {
                ++warmPixels;
            }
        }
    }
    GTEST_LOG_(INFO) << "Production smoke volume pixels visible/warm="
        << visiblePixels << '/' << warmPixels << " maxRGB="
        << maximumRed << '/' << maximumGreen << '/' << maximumBlue
        << " maxR-B=" << maximumWarmContrast
        << " grid=" << gridWidth << 'x' << gridHeight
        << " densityMass/centroidY=" << densityMass << '/'
        << (densityMass > 0.0 ? densityMomentY / densityMass /
                static_cast<double>(gridHeight) : 0.0)
        << " densityThirdShares(top/middle/bottom)="
        << (densityMass > 0.0 ? densityThirdMass[0] / densityMass : 0.0)
        << '/' << (densityMass > 0.0 ? densityThirdMass[1] / densityMass : 0.0)
        << '/' << (densityMass > 0.0 ? densityThirdMass[2] / densityMass : 0.0)
        << " outputAlphaCentroidY/thirdShares(top/middle/bottom)="
        << (outputAlphaMass > 0.0 ? outputAlphaMomentY / outputAlphaMass /
                static_cast<double>(outputHeight) : 0.0) << '/'
        << (outputAlphaMass > 0.0
                ? outputAlphaThirdMass[0] / outputAlphaMass : 0.0) << '/'
        << (outputAlphaMass > 0.0
                ? outputAlphaThirdMass[1] / outputAlphaMass : 0.0) << '/'
        << (outputAlphaMass > 0.0
                ? outputAlphaThirdMass[2] / outputAlphaMass : 0.0)
        << " outputLumaCentroidY/lumaThirdPixels(top/middle/bottom)="
        << (lumaMass > 0.0 ? lumaMomentY / lumaMass /
                static_cast<double>(outputHeight) : 0.0) << '/'
        << lumaThirdPixels[0] << '/' << lumaThirdPixels[1] << '/'
        << lumaThirdPixels[2] << " frame=" << frame
        << " topOutflowRate/band=" << safeTopOutflowRate << '/'
        << safeTopOutflowBand
        << " volumeDetail=" << settings.volumeDetailStrength
        << " frameWallMs=" << renderMilliseconds;
    const QString requestedOutputDirectory = qEnvironmentVariable(
        "ARTIFACT_SMOKE_PRODUCTION_OUTPUT_DIR_RND");
    const QString outputDirectory = requestedOutputDirectory.isEmpty()
        ? QDir::tempPath() : requestedOutputDirectory;
    ASSERT_TRUE(QDir().mkpath(outputDirectory));
    const QString smokeVolumeCapture = QDir(outputDirectory).filePath(
        QStringLiteral("production-smoke-frame-%1-%2x%3-grid-%4x%5-outflow-%6-detail-%7-source-r%8-n%9-d%10-vy%11.png")
            .arg(frame).arg(outputWidth).arg(outputHeight)
            .arg(gridWidth).arg(gridHeight)
            .arg(static_cast<int>(safeTopOutflowRate))
            .arg(static_cast<int>(settings.volumeDetailStrength * 100.0f))
            .arg(static_cast<int>(settings.emitterRadius * 100.0f))
            .arg(settings.emitterCount)
            .arg(static_cast<int>(settings.emitterDensity * 1000.0f))
            .arg(static_cast<int>(settings.emitterVelocityY * 100.0f)));
    EXPECT_TRUE(image.toQImage().save(smokeVolumeCapture, "PNG"));
    GTEST_LOG_(INFO) << "Production smoke volume capture="
        << smokeVolumeCapture.toStdString();
    EXPECT_GT(visiblePixels, 100u);
    EXPECT_GT(maximumWarmContrast, 0.05f);

    // Re-render the exact same simulated frame at several GPU shader detail
    // strengths. This isolates render-space breakup from solver resolution,
    // emission, exposure, and temporal evolution.
    const QImage baselineDetailImage = image.toQImage();
    constexpr std::array<float, 3> detailSweep{0.25f, 0.5f, 1.0f};
    for (const float detailStrength : detailSweep) {
        settings.volumeDetailStrength = detailStrength;
        renderer.clear();
        Artifact::renderLayerSmokeRuntime(
            runtime, &renderer, QRectF(0.0, 0.0, outputWidth, outputHeight),
            identity, 1.0f, frame, 30.0, settings);
        renderer.flushAndWait();
        const auto detailedImage = renderer.readbackToImageF32();
        ASSERT_EQ(detailedImage.width(), outputWidth);
        ASSERT_EQ(detailedImage.height(), outputHeight);
        const QImage detailedQImage = detailedImage.toQImage();
        std::size_t changedPixels = 0;
        std::array<std::size_t, 3> visibleThirdPixels{};
        for (int y = 0; y < outputHeight; ++y) {
            for (int x = 0; x < outputWidth; ++x) {
                const QRgb base = baselineDetailImage.pixel(x, y);
                const QRgb candidate = detailedQImage.pixel(x, y);
                const float delta = std::max({
                    static_cast<float>(std::abs(qRed(base) - qRed(candidate))),
                    static_cast<float>(std::abs(qGreen(base) - qGreen(candidate))),
                    static_cast<float>(std::abs(qBlue(base) - qBlue(candidate)))});
                if (delta > 1.0f) ++changedPixels;
                if (qRed(candidate) + qGreen(candidate) + qBlue(candidate) > 12) {
                    const auto third = std::min(2, 3 * y / outputHeight);
                    ++visibleThirdPixels[static_cast<std::size_t>(third)];
                }
            }
        }
        GTEST_LOG_(INFO) << "Production smoke GPU detail A/B strength="
            << detailStrength << " changedPixels=" << changedPixels
            << " visibleThirds(top/middle/bottom)="
            << visibleThirdPixels[0] << '/' << visibleThirdPixels[1] << '/'
            << visibleThirdPixels[2];
        EXPECT_GT(changedPixels, 0u);
        const QString detailCapture = QDir(outputDirectory).filePath(
            QStringLiteral("production-smoke-detail-%1-frame-%2-%3x%4-grid-%5x%6-outflow-%7-source-r%8-n%9-d%10-vy%11.png")
                .arg(static_cast<int>(detailStrength * 100.0f))
                .arg(frame).arg(outputWidth).arg(outputHeight)
                .arg(gridWidth).arg(gridHeight)
                .arg(static_cast<int>(safeTopOutflowRate))
                .arg(static_cast<int>(settings.emitterRadius * 100.0f))
                .arg(settings.emitterCount)
                .arg(static_cast<int>(settings.emitterDensity * 1000.0f))
                .arg(static_cast<int>(settings.emitterVelocityY * 100.0f)));
        EXPECT_TRUE(detailedQImage.save(detailCapture, "PNG"));
    }

    const auto sequentialSmoke = runtime.fluidSolver_->snapshot();
    const auto sequentialFlame = runtime.flameLifetime_;
    Artifact::advanceLayerSmokeRuntime(runtime, frame + 8, 30.0, settings);
    Artifact::advanceLayerSmokeRuntime(runtime, frame, 30.0, settings);
    ASSERT_NE(runtime.fluidSolver_, nullptr);
    const auto replayedSmoke = runtime.fluidSolver_->snapshot();
    const auto maximumFieldDelta = [](const auto& left, const auto& right) {
        if (left.size() != right.size())
            return std::numeric_limits<double>::infinity();
        double maximum = 0.0;
        for (std::size_t index = 0; index < left.size(); ++index) {
            maximum = std::max(maximum, std::abs(
                static_cast<double>(left[index]) -
                static_cast<double>(right[index])));
        }
        return maximum;
    };
    const double replayDensityDelta = maximumFieldDelta(
        sequentialSmoke.density, replayedSmoke.density);
    const double replayVelocityXDelta = maximumFieldDelta(
        sequentialSmoke.velocityX, replayedSmoke.velocityX);
    const double replayVelocityYDelta = maximumFieldDelta(
        sequentialSmoke.velocityY, replayedSmoke.velocityY);
    const double replayFlameDelta = maximumFieldDelta(
        sequentialFlame, runtime.flameLifetime_);
    GTEST_LOG_(INFO) << "Smoke top-outflow checkpoint replay max field deltas="
        << replayDensityDelta << '/' << replayVelocityXDelta << '/'
        << replayVelocityYDelta << '/' << replayFlameDelta;
    EXPECT_EQ(replayDensityDelta, 0.0);
    EXPECT_EQ(replayVelocityXDelta, 0.0);
    EXPECT_EQ(replayVelocityYDelta, 0.0);
    EXPECT_EQ(replayFlameDelta, 0.0);
}

TEST(ClonerGpuRenderContractTest,
     SmokeDepthVaryingVolumeRaymarchOnDiligentRenderer)
{
    using namespace Diligent;
    const auto readGridDimension = [](const char* name, const int fallback,
                                     const int minimum, const int maximum) {
        bool valid = false;
        const int value = qEnvironmentVariableIntValue(name, &valid);
        return valid ? std::clamp(value, minimum, maximum) : fallback;
    };
    const int gridWidth = readGridDimension(
        "ARTIFACT_SMOKE_VOLUME_GRID_WIDTH_RND", 96, 24, 192);
    const int gridHeight = readGridDimension(
        "ARTIFACT_SMOKE_VOLUME_GRID_HEIGHT_RND", 72, 24, 144);
    constexpr int volumeDepth = 32;
    const auto readOutputDimension = [](const char* name,
                                        const int fallback) {
        bool valid = false;
        const int value = qEnvironmentVariableIntValue(name, &valid);
        return valid ? std::clamp(value, 128, 1536) : fallback;
    };
    const int outputWidth = readOutputDimension(
        "ARTIFACT_SMOKE_VOLUME_WIDTH_RND", 384);
    const int outputHeight = readOutputDimension(
        "ARTIFACT_SMOKE_VOLUME_HEIGHT_RND", 288);
    const bool singleVolumeCapture = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_VOLUME_SINGLE_RND") != 0;
    bool flipVolumeFieldYValid = false;
    const int flipVolumeFieldYValue = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_VOLUME_FLIP_Y_RND", &flipVolumeFieldYValid);
    const bool flipVolumeFieldY = flipVolumeFieldYValid &&
        flipVolumeFieldYValue != 0;
    const QString outputDirectory = [&] {
        const QString dedicated = qEnvironmentVariable(
            "ARTIFACT_SMOKE_VOLUME_OUTPUT_DIR_RND");
        return dedicated.isEmpty()
            ? qEnvironmentVariable("ARTIFACT_SMOKE_GPU_OUTPUT_DIR")
            : dedicated;
    }();
    constexpr int captureFrame = 22;

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputWidth, outputHeight)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    auto device = renderer.device();
    auto context = renderer.immediateContext();
    ASSERT_NE(device, nullptr);
    ASSERT_NE(context, nullptr);

    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = gridWidth;
    settings.gridHeight = gridHeight;
    settings.solverIterations = 20;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.84f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.6f;
    settings.emitterVelocityY = -1.2f;
    const auto smokeFloatOverride = [](const char* name,
                                       const float fallback) {
        bool valid = false;
        const float value = qEnvironmentVariable(name).toFloat(&valid);
        return valid && std::isfinite(value) ? value : fallback;
    };
    settings.emitterY = smokeFloatOverride(
        "ARTIFACT_SMOKE_VOLUME_EMITTER_Y_RND", settings.emitterY);
    settings.emitterRadius = smokeFloatOverride(
        "ARTIFACT_SMOKE_VOLUME_EMITTER_RADIUS_RND", settings.emitterRadius);
    settings.emitterDensity = smokeFloatOverride(
        "ARTIFACT_SMOKE_VOLUME_EMITTER_DENSITY_RND", settings.emitterDensity);
    settings.emitterVelocityY = smokeFloatOverride(
        "ARTIFACT_SMOKE_VOLUME_EMITTER_VELOCITY_Y_RND",
        settings.emitterVelocityY);
    bool emitterCountValid = false;
    const int emitterCount = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_VOLUME_EMITTER_COUNT_RND", &emitterCountValid);
    if (emitterCountValid)
        settings.emitterCount = std::clamp(emitterCount, 1, 64);
    settings.emitterSwirl = 0.15f;
    settings.vorticity = 0.1f;
    settings.buoyancy = 0.0f;
    settings.emitterTemperature = 1.0f;
    settings.flameLifetimeSeconds = 1.0f;
    Artifact::LayerFluidRuntimeState runtime;
    constexpr float flameLifetimeSeconds = 1.0f;
    const auto fieldCellCount = static_cast<std::size_t>(
        gridWidth * gridHeight);
    for (int frame = 0; frame <= captureFrame; ++frame) {
        Artifact::advanceLayerSmokeRuntime(runtime, frame, 30.0, settings);
        ASSERT_NE(runtime.fluidSolver_, nullptr);
    }
    ASSERT_NE(runtime.fluidSolver_, nullptr);
    const auto* densityField = runtime.fluidSolver_->densityFieldData();
    const auto* flameField = runtime.flameLifetime_.constData();
    ASSERT_NE(densityField, nullptr);
    ASSERT_NE(flameField, nullptr);
    ASSERT_EQ(runtime.fluidSolver_->fieldElementCount(), fieldCellCount);

    double flameMass = 0.0;
    float maximumFlameLifetime = 0.0f;
    for (std::size_t index = 0; index < fieldCellCount; ++index) {
        const float value = flameField[index];
        flameMass += value;
        maximumFlameLifetime = std::max(maximumFlameLifetime, value);
    }
    const auto twoFieldUploadBytesPerFrame = fieldCellCount * 2 * sizeof(float);
    const auto expandedVolumeBytesPerFrame =
        twoFieldUploadBytesPerFrame * static_cast<std::size_t>(volumeDepth);
    GTEST_LOG_(INFO) << "GPU flame lifetime field frame=" << captureFrame
        << " lifespan=" << flameLifetimeSeconds
        << " mass=" << flameMass << " max=" << maximumFlameLifetime
        << " 2D-field-upload-bytes/frame=" << twoFieldUploadBytesPerFrame
        << " prior-CPU-volume-upload-bytes/frame="
        << expandedVolumeBytesPerFrame;
    EXPECT_GT(flameMass, 0.0);
    EXPECT_GT(maximumFlameLifetime, 0.0f);
    TextureDesc densityDesc;
    densityDesc.Name = "Smoke R&D source density field";
    densityDesc.Type = RESOURCE_DIM_TEX_2D;
    densityDesc.Width = gridWidth;
    densityDesc.Height = gridHeight;
    densityDesc.Format = TEX_FORMAT_R32_FLOAT;
    densityDesc.MipLevels = 1;
    densityDesc.Usage = USAGE_DEFAULT;
    densityDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData densitySubresource;
    densitySubresource.pData = const_cast<float*>(densityField);
    densitySubresource.Stride = gridWidth * sizeof(float);
    TextureData densityInitialData;
    densityInitialData.pSubResources = &densitySubresource;
    densityInitialData.NumSubresources = 1;
    RefCntAutoPtr<ITexture> densityTexture;
    device->CreateTexture(densityDesc, &densityInitialData, &densityTexture);
    ASSERT_NE(densityTexture, nullptr);
    auto* densitySRV = densityTexture->GetDefaultView(
        TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(densitySRV, nullptr);

    TextureDesc flameDesc;
    flameDesc.Name = "Smoke R&D source flame lifetime field";
    flameDesc.Type = RESOURCE_DIM_TEX_2D;
    flameDesc.Width = gridWidth;
    flameDesc.Height = gridHeight;
    flameDesc.Format = TEX_FORMAT_R32_FLOAT;
    flameDesc.MipLevels = 1;
    flameDesc.Usage = USAGE_DEFAULT;
    flameDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData flameSubresource;
    flameSubresource.pData = const_cast<float*>(flameField);
    flameSubresource.Stride = gridWidth * sizeof(float);
    TextureData flameInitialData;
    flameInitialData.pSubResources = &flameSubresource;
    flameInitialData.NumSubresources = 1;
    RefCntAutoPtr<ITexture> flameTexture;
    device->CreateTexture(flameDesc, &flameInitialData, &flameTexture);
    ASSERT_NE(flameTexture, nullptr);
    auto* flameSRV = flameTexture->GetDefaultView(
        TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(flameSRV, nullptr);

    TextureDesc outputDesc;
    outputDesc.Name = "Smoke R&D volume raymarch output";
    outputDesc.Type = RESOURCE_DIM_TEX_2D;
    outputDesc.Width = outputWidth;
    outputDesc.Height = outputHeight;
    outputDesc.MipLevels = 1;
    outputDesc.Format = TEX_FORMAT_RGBA8_UNORM;
    outputDesc.Usage = USAGE_DEFAULT;
    outputDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    RefCntAutoPtr<ITexture> outputTexture;
    device->CreateTexture(outputDesc, nullptr, &outputTexture);
    ASSERT_NE(outputTexture, nullptr);
    auto* outputUAV = outputTexture->GetDefaultView(
        TEXTURE_VIEW_UNORDERED_ACCESS);
    auto* outputSRV = outputTexture->GetDefaultView(
        TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(outputUAV, nullptr);
    ASSERT_NE(outputSRV, nullptr);

    const auto uploadField = [&](ITexture* texture, const float* field) {
        TextureSubResData subresource;
        subresource.pData = const_cast<float*>(field);
        subresource.Stride = gridWidth * sizeof(float);
        const Box updateBox(0, gridWidth, 0, gridHeight, 0, 1);
        context->UpdateTexture(
            texture, 0, 0, updateBox, subresource,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    };

    static constexpr char volumeShader[] = R"(
Texture2D<float> g_Density : register(t0);
Texture2D<float> g_Flame : register(t1);
RWTexture2D<float4> g_Output : register(u0);
SamplerState g_Sampler : register(s0);

cbuffer g_Params : register(b0) {
    uint Width;
    uint Height;
    uint Depth;
    uint ViewAxis;
    uint FieldWidth;
    uint FieldHeight;
    float DensityScale;
    float Extinction;
    float ShadowDensity;
    float AmbientFill;
    float LightX;
    float LightY;
    float LightZ;
    float PhaseG;
    float SmokeEnabled;
    float FlameEnabled;
    float FlameScale;
    float FlameGamma;
    float FlamePadding0;
    float FlamePadding1;
    float DensityDetailAmount;
    float DensityDetailScale;
    float DensityDetailTime;
    float DensityDetailPadding;
};

float hashValueNoise(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float valueNoise3(float3 p) {
    float3 cell = floor(p);
    float3 blend = frac(p);
    blend = blend * blend * (3.0 - 2.0 * blend);
    float c000 = hashValueNoise(cell + float3(0, 0, 0));
    float c100 = hashValueNoise(cell + float3(1, 0, 0));
    float c010 = hashValueNoise(cell + float3(0, 1, 0));
    float c110 = hashValueNoise(cell + float3(1, 1, 0));
    float c001 = hashValueNoise(cell + float3(0, 0, 1));
    float c101 = hashValueNoise(cell + float3(1, 0, 1));
    float c011 = hashValueNoise(cell + float3(0, 1, 1));
    float c111 = hashValueNoise(cell + float3(1, 1, 1));
    float z0 = lerp(lerp(c000, c100, blend.x),
                    lerp(c010, c110, blend.x), blend.y);
    float z1 = lerp(lerp(c001, c101, blend.x),
                    lerp(c011, c111, blend.x), blend.y);
    return lerp(z0, z1, blend.z);
}

float hashValueNoise2(float2 p) {
    return hashValueNoise(float3(p, 0.371));
}

float valueNoise2(float2 p) {
    float2 cell = floor(p);
    float2 blend = frac(p);
    blend = blend * blend * (3.0 - 2.0 * blend);
    float a = hashValueNoise2(cell);
    float b = hashValueNoise2(cell + float2(1.0, 0.0));
    float c = hashValueNoise2(cell + float2(0.0, 1.0));
    float d = hashValueNoise2(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, blend.x), lerp(c, d, blend.x), blend.y);
}

float2 warpedFieldUv(float3 position) {
    float zNorm = position.z * 2.0 - 1.0;
    float2 fieldCell = position.xy *
        (float2(FieldWidth, FieldHeight) - 1.0);
    float warpX = 2.5 * valueNoise2(float2(
        0.13 * fieldCell.x + 4.0 * zNorm,
        0.13 * fieldCell.y - 2.0 * zNorm + 7.0)) +
        0.8 * valueNoise2(float2(
            0.4 * fieldCell.x - 7.0 * zNorm,
            0.4 * fieldCell.y + 3.0 * zNorm));
    float warpY = 2.5 * valueNoise2(float2(
        0.13 * fieldCell.x - 2.0 * zNorm + 21.0,
        0.13 * fieldCell.y + 4.0 * zNorm - 17.0)) +
        0.8 * valueNoise2(float2(
            0.4 * fieldCell.x + 3.0 * zNorm,
            0.4 * fieldCell.y - 7.0 * zNorm));
    return position.xy + float2(
        warpX / max(1.0, float(FieldWidth - 1)),
        warpY / max(1.0, float(FieldHeight - 1)));
}

float depthEnvelope(float z) {
    float zNorm = z * 2.0 - 1.0;
    return exp(-2.5 * zNorm * zNorm);
}

float sampleSmokeVolume(float3 position) {
    float envelopeNormalization = 0.55;
    return g_Density.SampleLevel(
        g_Sampler, warpedFieldUv(position), 0) *
        depthEnvelope(position.z) / envelopeNormalization;
}

float sampleFlameVolume(float3 position) {
    return g_Flame.SampleLevel(
        g_Sampler, warpedFieldUv(position), 0) * depthEnvelope(position.z);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    float2 uv = (float2(id.xy) + 0.5) / float2(Width, Height);
    float screenY = FlamePadding1 > 0.5 ? 1.0 - uv.y : uv.y;
    float3 viewDir = ViewAxis == 0 ? float3(1.0, 0.0, 0.0)
                                   : float3(0.0, 0.0, 1.0);
    float3 lightDir = normalize(float3(LightX, LightY, LightZ));
    float g = clamp(PhaseG, -0.85, 0.85);
    float cosTheta = dot(-viewDir, -lightDir);
    float phaseDenom = max(1.0e-3, 1.0 + g * g - 2.0 * g * cosTheta);
    float phase = (1.0 - g * g) / pow(phaseDenom, 1.5);
    phase = clamp(phase * 0.5, 0.2, 1.4);
    float transmittance = 1.0;
    float flameTransmittance = 1.0;
    float3 radiance = 0.0;
    const uint shadowSteps = 8;
    const float shadowStep = 0.035;
    const float rayStep = 1.0 / float(Depth);
    [loop]
    for (uint step = 0; step < Depth; ++step) {
        float rayT = (float(step) + 0.5) * rayStep;
        float3 position = ViewAxis == 0
            ? float3(rayT, screenY, uv.x)
            : float3(uv.x, screenY, rayT);
        float density = 0.0;
        if (SmokeEnabled > 0.5) {
            float baseDensity = sampleSmokeVolume(position);
            float3 detailPosition = position * DensityDetailScale +
                float3(0.13 * DensityDetailTime, DensityDetailTime,
                       -0.07 * DensityDetailTime);
            float broadDetail = valueNoise3(detailPosition);
            float fineDetail = valueNoise3(detailPosition * 2.17 + 19.7);
            float detail = (broadDetail - 0.5) * 0.75 +
                           (fineDetail - 0.5) * 0.25;
            density = baseDensity * DensityScale * max(
                0.0, 1.0 + DensityDetailAmount * detail);
        }
        float flameValue = FlameEnabled > 0.5
            ? pow(saturate(sampleFlameVolume(position)),
                  max(0.1, FlameGamma)) * FlameScale
            : 0.0;
        float flameAlpha = 1.0 - exp(-flameValue * rayStep);
        radiance += transmittance * flameAlpha *
            float3(1.0, 0.24, 0.025) * 2.5;
        flameTransmittance *= 1.0 - flameAlpha;
        if (density > 1.0e-5) {
            float opticalDepth = 0.0;
            [unroll]
            for (uint shadowStepIndex = 1;
                 shadowStepIndex <= shadowSteps; ++shadowStepIndex) {
                float3 shadowPosition = position + lightDir *
                    (float(shadowStepIndex) * shadowStep);
                if (any(shadowPosition < 0.0) ||
                    any(shadowPosition > 1.0)) break;
                opticalDepth += sampleSmokeVolume(shadowPosition) *
                    ShadowDensity * shadowStep;
            }
            float lightTransmittance = exp(-opticalDepth);
            float illumination = AmbientFill +
                (1.0 - AmbientFill) * lightTransmittance * phase;
            float alpha = 1.0 - exp(-density * Extinction * rayStep);
            float scatterWeight = FlamePadding0 > 0.5
                ? saturate(density * 10.0) : 1.0;
            radiance += transmittance * alpha * illumination * scatterWeight *
                float3(0.78, 0.84, 0.92);
            transmittance *= 1.0 - alpha;
            if (transmittance < 0.01) break;
        }
    }
    g_Output[id.xy] = float4(
        radiance, 1.0 - transmittance * flameTransmittance);
}
)";

    ShaderCreateInfo shaderInfo;
    shaderInfo.Desc.Name = "Smoke R&D depth-varying volume raymarch";
    shaderInfo.Desc.ShaderType = SHADER_TYPE_COMPUTE;
    shaderInfo.EntryPoint = "CSMain";
    shaderInfo.Source = volumeShader;
    shaderInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    RefCntAutoPtr<IShader> computeShader;
    device->CreateShader(shaderInfo, &computeShader);
    ASSERT_NE(computeShader, nullptr);

    ComputePipelineStateCreateInfo pipelineInfo;
    pipelineInfo.PSODesc.Name = "Smoke R&D depth-varying volume PSO";
    pipelineInfo.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    pipelineInfo.pCS = computeShader;
    RefCntAutoPtr<IPipelineState> pipeline;
    device->CreateComputePipelineState(pipelineInfo, &pipeline);
    ASSERT_NE(pipeline, nullptr);

    struct alignas(16) VolumeParameters final {
        Uint32 width;
        Uint32 height;
        Uint32 depth;
        Uint32 viewAxis;
        Uint32 fieldWidth;
        Uint32 fieldHeight;
        float densityScale;
        float extinction;
        float shadowDensity;
        float ambientFill;
        float lightX;
        float lightY;
        float lightZ;
        float phaseG;
        float smokeEnabled;
        float flameEnabled;
        float flameScale;
        float flameGamma;
        float padding0;
        float padding1;
        float densityDetailAmount;
        float densityDetailScale;
        float densityDetailTime;
        float densityDetailPadding;
    };
static_assert(sizeof(VolumeParameters) == 96);
    VolumeParameters parameters{
        static_cast<Uint32>(outputWidth), static_cast<Uint32>(outputHeight),
        static_cast<Uint32>(volumeDepth), 1,
        static_cast<Uint32>(gridWidth), static_cast<Uint32>(gridHeight),
        6.0f, 2.0f, 1.0f, 0.28f,
        -0.42f, 0.62f, 0.66f, 0.25f, 1.0f, 0.0f, 3.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 18.0f, 0.0f, 0.0f};
    BufferDesc parameterDesc;
    parameterDesc.Name = "Smoke R&D volume raymarch parameters";
    parameterDesc.Size = sizeof(VolumeParameters);
    parameterDesc.Usage = USAGE_DEFAULT;
    parameterDesc.BindFlags = BIND_UNIFORM_BUFFER;
    RefCntAutoPtr<IBuffer> parameterBuffer;
    BufferData parameterInitialData{&parameters, sizeof(parameters)};
    device->CreateBuffer(parameterDesc, &parameterInitialData, &parameterBuffer);
    ASSERT_NE(parameterBuffer, nullptr);

    SamplerDesc samplerDesc;
    samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
    RefCntAutoPtr<ISampler> sampler;
    device->CreateSampler(samplerDesc, &sampler);
    ASSERT_NE(sampler, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> srb;
    pipeline->CreateShaderResourceBinding(&srb, true);
    ASSERT_NE(srb, nullptr);
    auto* densityVariable = srb->GetVariableByName(
        SHADER_TYPE_COMPUTE, "g_Density");
    auto* flameVariable = srb->GetVariableByName(
        SHADER_TYPE_COMPUTE, "g_Flame");
    auto* outputVariable = srb->GetVariableByName(
        SHADER_TYPE_COMPUTE, "g_Output");
    auto* samplerVariable = srb->GetVariableByName(
        SHADER_TYPE_COMPUTE, "g_Sampler");
    auto* parameterVariable = srb->GetVariableByName(
        SHADER_TYPE_COMPUTE, "g_Params");
    ASSERT_NE(densityVariable, nullptr);
    ASSERT_NE(flameVariable, nullptr);
    ASSERT_NE(outputVariable, nullptr);
    ASSERT_NE(samplerVariable, nullptr);
    ASSERT_NE(parameterVariable, nullptr);
    densityVariable->Set(densitySRV);
    flameVariable->Set(flameSRV);
    outputVariable->Set(outputUAV);
    samplerVariable->Set(sampler);
    parameterVariable->Set(parameterBuffer);

    std::vector<double> raymarchMilliseconds;
    const auto render = [&](const Uint32 viewAxis, const float ambientFill,
                            const float phaseG, const bool smokeEnabled,
                            const bool flameEnabled,
                            const float flameGamma = 1.0f,
                            const float flameScale = 3.0f,
                            const float densityDetailAmount = 0.0f,
                            const float densityDetailScale = 18.0f,
                            const float densityDetailTime = 0.0f,
                            const bool densityWeightedSmoke = false) {
        const auto start = std::chrono::steady_clock::now();
        parameters.viewAxis = viewAxis;
        parameters.ambientFill = ambientFill;
        parameters.phaseG = phaseG;
        parameters.smokeEnabled = smokeEnabled ? 1.0f : 0.0f;
        parameters.flameEnabled = flameEnabled ? 1.0f : 0.0f;
        parameters.flameGamma = flameGamma;
        parameters.flameScale = flameScale;
        parameters.padding0 = densityWeightedSmoke ? 1.0f : 0.0f;
        parameters.padding1 = flipVolumeFieldY ? 1.0f : 0.0f;
        parameters.densityDetailAmount = densityDetailAmount;
        parameters.densityDetailScale = densityDetailScale;
        parameters.densityDetailTime = densityDetailTime;
        context->UpdateBuffer(parameterBuffer, 0, sizeof(parameters),
            &parameters, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->SetPipelineState(pipeline);
        context->CommitShaderResources(
            srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DispatchComputeAttribs dispatch;
        dispatch.ThreadGroupCountX = (outputWidth + 7) / 8;
        dispatch.ThreadGroupCountY = (outputHeight + 7) / 8;
        dispatch.ThreadGroupCountZ = 1;
        context->DispatchCompute(dispatch);
        renderer.flushAndWait();
        auto image = renderer.readbackTextureViewToImage(outputSRV);
        raymarchMilliseconds.push_back(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count());
        return image;
    };

    if (singleVolumeCapture) {
        const auto image = render(1, 0.28f, 0.25f, true, false);
        ASSERT_FALSE(image.isNull());
        std::size_t visiblePixels = 0;
        double lumaSum = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = image.pixel(x, y);
                const double luma = 0.2126 * qRed(pixel) +
                    0.7152 * qGreen(pixel) + 0.0722 * qBlue(pixel);
                if (luma > 2.0) {
                    ++visiblePixels;
                    lumaSum += luma;
                }
            }
        }
        EXPECT_GT(visiblePixels, 0u);
        GTEST_LOG_(INFO) << "Diligent smoke volume single capture size="
            << outputWidth << 'x' << outputHeight
            << " visiblePixels=" << visiblePixels
            << " meanVisibleLuma=" << (visiblePixels > 0
                ? lumaSum / static_cast<double>(visiblePixels) : 0.0)
            << " dispatchFlushReadbackMs=" << raymarchMilliseconds.back();
        if (!outputDirectory.isEmpty()) {
            ASSERT_TRUE(QDir().mkpath(outputDirectory));
            EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(
                QStringLiteral("smoke-volume-single.png")), "PNG"));
        }
        return;
    }

    const auto frontLit = render(1, 0.28f, 0.25f, true, false);
    const auto densityWeightedFront = render(
        1, 0.28f, 0.25f, true, false, 1.0f, 3.0f, 0.0f, 18.0f, 0.0f, true);
    const auto frontAmbient = render(1, 0.55f, 0.0f, true, false);
    const auto sideLit = render(0, 0.28f, 0.25f, true, false);
    const auto flameOnly = render(1, 0.28f, 0.25f, false, true);
    const auto shapedFlameOnly = render(
        1, 0.28f, 0.25f, false, true, 2.0f, 9.0f);
    const auto combined = render(1, 0.28f, 0.25f, true, true);
    const auto shapedCombined = render(
        1, 0.28f, 0.25f, true, true, 2.0f, 9.0f);
    const auto detailedCombined = render(
        1, 0.28f, 0.25f, true, true, 2.0f, 9.0f, 1.0f, 18.0f, 0.35f);
    const auto strongDetailedCombined = render(
        1, 0.28f, 0.25f, true, true, 2.0f, 9.0f, 2.4f, 42.0f, 0.35f);
    const auto highDetailCombined = render(
        1, 0.28f, 0.25f, true, true, 2.0f, 9.0f, 6.0f, 14.0f, 0.35f);
    // Hold the simulated fields fixed and advance only render-space detail.
    // This isolates temporal shimmer caused by the noise treatment itself.
    std::array<QImage, 4> animatedDetailFrames;
    for (std::size_t frame = 0; frame < animatedDetailFrames.size(); ++frame) {
        animatedDetailFrames[frame] = render(
            1, 0.28f, 0.25f, true, true, 2.0f, 9.0f, 6.0f, 14.0f,
            0.35f + static_cast<float>(frame) * (1.0f / 30.0f));
    }

    // Exercise advancing production solver fields through the same reusable
    // GPU volume textures, then seek back and require an identical image.
    const std::vector<float> referenceDensityField(
        densityField, densityField + fieldCellCount);
    const std::vector<float> referenceFlameField(
        flameField, flameField + fieldCellCount);
    std::array<QImage, 4> runtimeMotionFrames;
    runtimeMotionFrames[0] = combined;
    for (std::size_t frameOffset = 1;
         frameOffset < runtimeMotionFrames.size(); ++frameOffset) {
        const int simulationFrame = captureFrame +
            static_cast<int>(frameOffset);
        Artifact::advanceLayerSmokeRuntime(
            runtime, simulationFrame, 30.0, settings);
        densityField = runtime.fluidSolver_->densityFieldData();
        flameField = runtime.flameLifetime_.constData();
        ASSERT_NE(densityField, nullptr);
        ASSERT_NE(flameField, nullptr);
        uploadField(densityTexture, densityField);
        uploadField(flameTexture, flameField);
        runtimeMotionFrames[frameOffset] = render(
            1, 0.28f, 0.25f, true, true);
    }
    Artifact::advanceLayerSmokeRuntime(runtime, captureFrame, 30.0, settings);
    densityField = runtime.fluidSolver_->densityFieldData();
    flameField = runtime.flameLifetime_.constData();
    ASSERT_NE(densityField, nullptr);
    ASSERT_NE(flameField, nullptr);
    double densitySeekMaxDifference = 0.0;
    double flameSeekMaxDifference = 0.0;
    for (std::size_t index = 0; index < fieldCellCount; ++index) {
        densitySeekMaxDifference = std::max(
            densitySeekMaxDifference,
            std::abs(static_cast<double>(densityField[index]) -
                     referenceDensityField[index]));
        flameSeekMaxDifference = std::max(
            flameSeekMaxDifference,
            std::abs(static_cast<double>(flameField[index]) -
                     referenceFlameField[index]));
    }
    uploadField(densityTexture, densityField);
    uploadField(flameTexture, flameField);
    const auto runtimeSeekFrame = render(1, 0.28f, 0.25f, true, true);

    ASSERT_FALSE(frontLit.isNull());
    ASSERT_FALSE(densityWeightedFront.isNull());
    EXPECT_NE(frontLit, densityWeightedFront);
    ASSERT_FALSE(frontAmbient.isNull());
    ASSERT_FALSE(sideLit.isNull());
    ASSERT_FALSE(flameOnly.isNull());
    ASSERT_FALSE(shapedFlameOnly.isNull());
    ASSERT_FALSE(combined.isNull());
    ASSERT_FALSE(shapedCombined.isNull());
    ASSERT_FALSE(detailedCombined.isNull());
    ASSERT_FALSE(strongDetailedCombined.isNull());
    ASSERT_FALSE(highDetailCombined.isNull());
    for (const auto& image : animatedDetailFrames)
        ASSERT_FALSE(image.isNull());
    for (const auto& image : runtimeMotionFrames)
        ASSERT_FALSE(image.isNull());
    ASSERT_FALSE(runtimeSeekFrame.isNull());
    ASSERT_EQ(frontLit.size(), QSize(outputWidth, outputHeight));
    const auto imageMetrics = [](const QImage& image) {
        double lumaSum = 0.0;
        double lumaSquared = 0.0;
        double redSum = 0.0;
        double blueSum = 0.0;
        std::size_t covered = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                const double luma = 0.2126 * color.red() +
                    0.7152 * color.green() + 0.0722 * color.blue();
                lumaSum += luma;
                lumaSquared += luma * luma;
                redSum += color.red();
                blueSum += color.blue();
                if (color.alpha() > 8) ++covered;
            }
        }
        const double count = static_cast<double>(image.width()) * image.height();
        const double mean = lumaSum / count;
        return std::array<double, 5>{mean,
            std::sqrt(std::max(0.0, lumaSquared / count - mean * mean)),
            static_cast<double>(covered), redSum / count, blueSum / count};
    };
    const auto litMetrics = imageMetrics(frontLit);
    const auto weightedMetrics = imageMetrics(densityWeightedFront);
    GTEST_LOG_(INFO) << "smoke scatter constant/density-weighted mean="
        << litMetrics[0] << '/' << weightedMetrics[0];
    const auto ambientMetrics = imageMetrics(frontAmbient);
    const auto verticalVisibility = [](const QImage& image) {
        std::array<std::uint64_t, 3> covered{};
        std::array<double, 3> coveredLuma{};
        std::array<std::uint64_t, 3> pixelCount{};
        for (int y = 0; y < image.height(); ++y) {
            const std::size_t third = std::min<std::size_t>(2,
                3 * static_cast<std::size_t>(y) /
                    static_cast<std::size_t>(image.height()));
            for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                const double alpha = color.alphaF();
                const double luma = (0.2126 * color.red() +
                    0.7152 * color.green() + 0.0722 * color.blue()) / 255.0;
                ++pixelCount[third];
                if (alpha <= 0.01) continue;
                ++covered[third];
                coveredLuma[third] += luma;
            }
        }
        std::array<double, 6> metrics{};
        for (std::size_t third = 0; third < metrics.size() / 2; ++third) {
            metrics[third] = pixelCount[third] > 0
                ? static_cast<double>(covered[third]) / pixelCount[third]
                : 0.0;
            metrics[third + 3] = covered[third] > 0
                ? coveredLuma[third] / covered[third] : 0.0;
        }
        return metrics;
    };
    const auto litVertical = verticalVisibility(frontLit);
    const auto ambientVertical = verticalVisibility(frontAmbient);
    GTEST_LOG_(INFO) << "GPU smoke vertical visibility top/middle/bottom"
        << " covered=" << litVertical[0] << '/' << litVertical[1] << '/'
        << litVertical[2] << " ambient=" << ambientVertical[0] << '/'
        << ambientVertical[1] << '/' << ambientVertical[2]
        << " covered-luma=" << litVertical[3] << '/' << litVertical[4] << '/'
        << litVertical[5] << " ambient=" << ambientVertical[3] << '/'
        << ambientVertical[4] << '/' << ambientVertical[5];
    const auto sideMetrics = imageMetrics(sideLit);
    const auto flameMetrics = imageMetrics(flameOnly);
    const auto shapedFlameMetrics = imageMetrics(shapedFlameOnly);
    const auto combinedMetrics = imageMetrics(combined);
    const auto shapedCombinedMetrics = imageMetrics(shapedCombined);
    const auto detailedCombinedMetrics = imageMetrics(detailedCombined);
    const auto strongDetailedCombinedMetrics = imageMetrics(
        strongDetailedCombined);
    const auto highDetailMetrics = imageMetrics(highDetailCombined);
    const auto meanAbsoluteLumaDelta = [](const QImage& a, const QImage& b) {
        double sum = 0.0;
        for (int y = 0; y < a.height(); ++y) {
            for (int x = 0; x < a.width(); ++x) {
                const QColor ca = a.pixelColor(x, y);
                const QColor cb = b.pixelColor(x, y);
                const double la = 0.2126 * ca.red() + 0.7152 * ca.green() +
                                  0.0722 * ca.blue();
                const double lb = 0.2126 * cb.red() + 0.7152 * cb.green() +
                                  0.0722 * cb.blue();
                sum += std::abs(la - lb);
            }
        }
        return sum / (static_cast<double>(a.width()) * a.height());
    };
    std::array<double, 3> animatedDetailDelta{};
    for (std::size_t frame = 0; frame < animatedDetailDelta.size(); ++frame)
        animatedDetailDelta[frame] = meanAbsoluteLumaDelta(
            animatedDetailFrames[frame], animatedDetailFrames[frame + 1]);
    std::array<double, 3> runtimeMotionDelta{};
    for (std::size_t frame = 0; frame < runtimeMotionDelta.size(); ++frame)
        runtimeMotionDelta[frame] = meanAbsoluteLumaDelta(
            runtimeMotionFrames[frame], runtimeMotionFrames[frame + 1]);
    EXPECT_NE(runtimeMotionFrames[0], runtimeMotionFrames[1]);
    EXPECT_NE(runtimeMotionFrames[1], runtimeMotionFrames[2]);
    EXPECT_NE(runtimeMotionFrames[2], runtimeMotionFrames[3]);
    EXPECT_DOUBLE_EQ(densitySeekMaxDifference, 0.0);
    EXPECT_DOUBLE_EQ(flameSeekMaxDifference, 0.0);
    EXPECT_EQ(runtimeSeekFrame, runtimeMotionFrames[0]);
    GTEST_LOG_(INFO) << "GPU smoke 2.5D raymarch frame=" << captureFrame
        << " front-lit mean/std/covered=" << litMetrics[0] << '/'
        << litMetrics[1] << '/' << litMetrics[2]
        << " front-ambient=" << ambientMetrics[0] << '/'
        << ambientMetrics[1] << '/' << ambientMetrics[2]
        << " side-lit=" << sideMetrics[0] << '/' << sideMetrics[1] << '/'
        << sideMetrics[2]
        << " flame-only mean/std/covered/red/blue=" << flameMetrics[0] << '/'
        << flameMetrics[1] << '/' << flameMetrics[2] << '/'
        << flameMetrics[3] << '/' << flameMetrics[4]
        << " flame-gamma-2=" << shapedFlameMetrics[0] << '/'
        << shapedFlameMetrics[1] << '/' << shapedFlameMetrics[2] << '/'
        << shapedFlameMetrics[3] << '/' << shapedFlameMetrics[4]
        << " combined=" << combinedMetrics[0] << '/' << combinedMetrics[1]
        << '/' << combinedMetrics[2]
        << " shaped-combined=" << shapedCombinedMetrics[0] << '/'
        << shapedCombinedMetrics[1] << '/' << shapedCombinedMetrics[2]
        << " detailed-combined=" << detailedCombinedMetrics[0] << '/'
        << detailedCombinedMetrics[1] << '/' << detailedCombinedMetrics[2]
        << " strong-detail=" << strongDetailedCombinedMetrics[0] << '/'
        << strongDetailedCombinedMetrics[1] << '/'
        << strongDetailedCombinedMetrics[2]
        << " high-detail=" << highDetailMetrics[0] << '/'
        << highDetailMetrics[1] << '/' << highDetailMetrics[2]
        << " animated-detail adjacent-luma-mae="
        << animatedDetailDelta[0] << '/' << animatedDetailDelta[1] << '/'
        << animatedDetailDelta[2]
        << " runtime-field adjacent-luma-mae="
        << runtimeMotionDelta[0] << '/' << runtimeMotionDelta[1] << '/'
        << runtimeMotionDelta[2]
        << " runtime-seek max-density/flame/image-difference="
        << densitySeekMaxDifference << '/' << flameSeekMaxDifference << '/'
        << (runtimeSeekFrame == runtimeMotionFrames[0] ? 0 : 1)
        << " raymarch-ms baseline/detail/strong="
        << raymarchMilliseconds[5] << '/' << raymarchMilliseconds[7] << '/'
        << raymarchMilliseconds[8];
    EXPECT_GT(litMetrics[2], 100.0);
    EXPECT_GT(sideMetrics[2], 100.0);
    EXPECT_GT(flameMetrics[2], 100.0);
    EXPECT_GT(flameMetrics[3], flameMetrics[4]);
    EXPECT_GT(shapedFlameMetrics[2], 100.0);
    EXPECT_GT(shapedFlameMetrics[3], shapedFlameMetrics[4]);
    EXPECT_GT(combinedMetrics[2], 100.0);
    EXPECT_NE(combined, frontLit);
    EXPECT_NE(shapedCombined, combined);
    EXPECT_NE(detailedCombined, shapedCombined);
    EXPECT_NE(strongDetailedCombined, shapedCombined);
    EXPECT_NE(highDetailCombined, shapedCombined);

    if (!outputDirectory.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(outputDirectory));
        EXPECT_TRUE(densityWeightedFront.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-density-weighted-front.png")), "PNG"));
        EXPECT_TRUE(frontLit.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-front-lit.png")), "PNG"));
        EXPECT_TRUE(frontAmbient.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-front-ambient.png")), "PNG"));
        EXPECT_TRUE(sideLit.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-side-lit.png")), "PNG"));
        EXPECT_TRUE(flameOnly.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-flame-only.png")), "PNG"));
        EXPECT_TRUE(shapedFlameOnly.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-flame-gamma-2.png")), "PNG"));
        EXPECT_TRUE(combined.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-smoke-and-flame.png")), "PNG"));
        EXPECT_TRUE(shapedCombined.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-smoke-and-flame-gamma-2.png")), "PNG"));
        EXPECT_TRUE(detailedCombined.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-smoke-and-flame-detail-r1.png")), "PNG"));
        EXPECT_TRUE(strongDetailedCombined.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-smoke-and-flame-detail-r2.png")), "PNG"));
        EXPECT_TRUE(highDetailCombined.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-2p5d-smoke-and-flame-detail-r3.png")), "PNG"));
        for (std::size_t frame = 0; frame < animatedDetailFrames.size(); ++frame) {
            EXPECT_TRUE(animatedDetailFrames[frame].save(
                QDir(outputDirectory).filePath(QStringLiteral(
                    "volume-2p5d-smoke-and-flame-animated-detail-%1.png")
                    .arg(static_cast<int>(frame))), "PNG"));
        }
        for (std::size_t frame = 0; frame < runtimeMotionFrames.size(); ++frame) {
            EXPECT_TRUE(runtimeMotionFrames[frame].save(
                QDir(outputDirectory).filePath(QStringLiteral(
                    "volume-production-runtime-frame-%1.png")
                    .arg(captureFrame + static_cast<int>(frame))), "PNG"));
        }
        EXPECT_TRUE(runtimeSeekFrame.save(QDir(outputDirectory).filePath(
            QStringLiteral("volume-production-runtime-seek-frame-%1.png")
                .arg(captureFrame)), "PNG"));
    }
}

TEST(ClonerGpuRenderContractTest,
     PyroGpuVolumePreservesLowDensityAlpha)
{
    using namespace Diligent;
    using namespace ArtifactCore::RayTrace;
    constexpr int dimension = 8;
    std::array<float, dimension * dimension * dimension> density{};
    std::array<float, dimension * dimension * dimension> temperature{};
    density.fill(0.00005f);
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(32, 32))
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    auto device = renderer.device();
    auto context = renderer.immediateContext();
    ASSERT_NE(device, nullptr);
    ASSERT_NE(context, nullptr);
    GpuVolumeRenderer volume;
    ASSERT_TRUE(volume.initialize(device, context));
    const VolumeFieldSet fields{
        .density = {density.data(), {dimension, dimension, dimension}},
        .temperature = {temperature.data(), {dimension, dimension, dimension}}};
    GpuVolumeRenderSettings settings;
    settings.outputWidth = 32;
    settings.outputHeight = 32;
    settings.raySteps = 64;
    ASSERT_TRUE(volume.render(context, fields, TransferFunction::makeSmoke(), settings));
    auto* view = volume.outputSRV();
    ASSERT_NE(view, nullptr);
    auto* source = view->GetTexture();
    ASSERT_NE(source, nullptr);
    ASSERT_EQ(source->GetDesc().Format, TEX_FORMAT_RGBA16_FLOAT);
    TextureDesc desc = source->GetDesc();
    desc.Name = "Low density volume readback";
    desc.Usage = USAGE_STAGING;
    desc.BindFlags = BIND_NONE;
    desc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<ITexture> staging;
    device->CreateTexture(desc, nullptr, &staging);
    ASSERT_NE(staging, nullptr);
    const double expectedAlpha = 1.0 - std::exp(-0.00005 * 30.0);
    for (int steps : {16, 64, 256}) {
        settings.raySteps = steps;
        ASSERT_TRUE(volume.render(context, fields, TransferFunction::makeSmoke(), settings));
        CopyTextureAttribs copy;
        copy.pSrcTexture = source;
        copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        copy.pDstTexture = staging;
        copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        context->CopyTexture(copy);
        renderer.flushAndWait();
        MappedTextureSubresource mapped{};
        context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
        ASSERT_NE(mapped.pData, nullptr);
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData) + 16 * mapped.Stride;
        const auto* pixel = reinterpret_cast<const std::uint16_t*>(row) + 16 * 4;
        const std::uint16_t alphaBits = pixel[3];
        const std::uint16_t greenBits = pixel[1];
        context->UnmapTextureSubresource(staging, 0, 0);
        const auto positiveHalf = [](std::uint16_t bits) {
            const unsigned exponent = (bits >> 10u) & 31u;
            const unsigned mantissa = bits & 1023u;
            if ((bits & 0x8000u) || exponent == 31u) return -1.0;
            return exponent == 0
                ? std::ldexp(static_cast<double>(mantissa), -24)
                : std::ldexp(1.0 + mantissa / 1024.0, static_cast<int>(exponent) - 15);
        };
        const double alpha = positiveHalf(alphaBits);
        const double green = positiveHalf(greenBits);
        EXPECT_NEAR(green, 0.00005 * 3.0, 0.000002) << "ray steps=" << steps;
        EXPECT_NEAR(alpha, expectedAlpha, 0.00003) << "ray steps=" << steps;
        GTEST_LOG_(INFO) << "low density volume steps=" << steps
                         << " alpha=" << alpha << " expected=" << expectedAlpha
                         << " straight-green=" << green;
    }
}

TEST(ClonerGpuRenderContractTest, PyroRenderDetailMovesDifferentlyFromTranslatingDensity)
{
    using namespace ArtifactCore::RayTrace;
    constexpr int dimension = 24;
    constexpr int output = 192;
    constexpr int gridShift = 4;
    constexpr int pixelShift = output * gridShift / dimension;
    constexpr std::size_t count = dimension * dimension * dimension;
    std::array<float, count> densityA{}, densityB{}, temperature{};
    for (int z = 0; z < dimension; ++z)
        for (int y = 0; y < dimension; ++y)
            for (int x = 0; x < dimension; ++x) {
                const auto i = static_cast<std::size_t>(x) + dimension *
                    (static_cast<std::size_t>(y) + dimension * static_cast<std::size_t>(z));
                const auto value = [&](int centerX) {
                    const float dx = x - static_cast<float>(centerX);
                    const float dy = y - 11.5f, dz = z - 11.5f;
                    return 0.06f * std::exp(-(dx * dx + dy * dy + dz * dz) / 14.0f);
                };
                densityA[i] = value(8);
                densityB[i] = value(8 + gridShift);
            }
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(output, output))
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    GpuVolumeRenderer volume;
    ASSERT_TRUE(volume.initialize(renderer.device(), renderer.immediateContext()));
    GpuVolumeRenderSettings settings;
    settings.outputWidth = output;
    settings.outputHeight = output;
    settings.raySteps = 64;
    settings.detailScale = 12.0f;
    settings.detailSeed = 2026;
    const auto render = [&](auto& density, float detail) {
        settings.detailStrength = detail;
        const VolumeFieldSet fields{
            .density = {density.data(), {dimension, dimension, dimension}},
            .temperature = {temperature.data(), {dimension, dimension, dimension}}};
        EXPECT_TRUE(volume.render(renderer.immediateContext(), fields,
                                  TransferFunction::makeSmoke(), settings));
        renderer.flushAndWait();
        return renderer.readbackTextureViewToImage(volume.outputSRV());
    };
    const auto baseA = render(densityA, 0.0f);
    const auto baseB = render(densityB, 0.0f);
    const auto detailA = render(densityA, 1.0f);
    const auto detailB = render(densityB, 1.0f);
    settings.detailOffsetX = -static_cast<float>(gridShift) / dimension;
    const auto advectedDetailB = render(densityB, 1.0f);
    ASSERT_FALSE(baseA.isNull());
    ASSERT_FALSE(baseB.isNull());
    ASSERT_FALSE(detailA.isNull());
    ASSERT_FALSE(detailB.isNull());
    ASSERT_FALSE(advectedDetailB.isNull());
    const auto translatedAlphaError = [&](const QImage& first, const QImage& second) {
        double sum = 0.0;
        std::size_t pixels = 0;
        for (int y = 0; y < output; ++y)
            for (int x = 0; x < output - pixelShift; ++x) {
                sum += std::abs(first.pixelColor(x, y).alpha() -
                                second.pixelColor(x + pixelShift, y).alpha());
                ++pixels;
            }
        return sum / pixels;
    };
    const double baseError = translatedAlphaError(baseA, baseB);
    const double detailError = translatedAlphaError(detailA, detailB);
    const double advectedError = translatedAlphaError(detailA, advectedDetailB);
    GTEST_LOG_(INFO) << "Pyro translation alpha mean error base/fixed/offset="
                     << baseError << '/' << detailError << '/' << advectedError;
    EXPECT_LT(baseError, 1.0);
    EXPECT_TRUE(std::isfinite(detailError));
    EXPECT_LT(advectedError, 0.1);
    EXPECT_LT(advectedError, detailError * 0.1);
}

TEST(ClonerGpuRenderContractTest,
     PyroGpuVolumeRendererProducesDistinctTransferLooks)
{
    using namespace ArtifactCore::RayTrace;
    constexpr int fieldWidth = 24;
    constexpr int fieldHeight = 24;
    constexpr int fieldDepth = 24;
    constexpr int outputSize = 96;
    constexpr std::size_t cellCount = fieldWidth * fieldHeight * fieldDepth;
    std::array<float, cellCount> density{};
    std::array<float, cellCount> temperature{};
    for (int z = 0; z < fieldDepth; ++z) {
        for (int y = 0; y < fieldHeight; ++y) {
            for (int x = 0; x < fieldWidth; ++x) {
                const float nx = (static_cast<float>(x) + 0.5f) /
                                     static_cast<float>(fieldWidth) * 2.0f - 1.0f;
                const float ny = (static_cast<float>(y) + 0.5f) /
                                     static_cast<float>(fieldHeight) * 2.0f - 1.0f;
                const float nz = (static_cast<float>(z) + 0.5f) /
                                     static_cast<float>(fieldDepth) * 2.0f - 1.0f;
                const float radius = std::sqrt(nx * nx + ny * ny + nz * nz);
                const auto index = static_cast<std::size_t>(x) +
                    fieldWidth * (static_cast<std::size_t>(y) +
                                  fieldHeight * static_cast<std::size_t>(z));
                density[index] = std::max(0.0f, 1.0f - radius) *
                                 (0.72f + 0.28f * std::sin(8.0f * nz));
                temperature[index] = std::max(0.0f, 1.0f - radius * 1.2f);
            }
        }
    }

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputSize, outputSize)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    GTEST_LOG_(INFO) << "Pyro GPU probe: headless renderer ready";
    auto device = renderer.device();
    auto context = renderer.immediateContext();
    ASSERT_NE(device, nullptr);
    ASSERT_NE(context, nullptr);

    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(device, context));
    GTEST_LOG_(INFO) << "Pyro GPU probe: raymarch pipeline ready";
    const VolumeFieldSet fields{
        .density = {density.data(), {fieldWidth, fieldHeight, fieldDepth}},
        .temperature = {temperature.data(), {fieldWidth, fieldHeight, fieldDepth}}};
    const GpuVolumeRenderSettings settings{
        .outputWidth = outputSize,
        .outputHeight = outputSize,
        .raySteps = 72,
        .densityScale = 1.0f,
        .temperatureScale = 1.0f,
        .opacity = 1.0f};

    const auto render = [&](const TransferFunction& transfer) {
        GTEST_LOG_(INFO) << "Pyro GPU probe: dispatch transfer="
                         << static_cast<int>(transfer.kind);
        EXPECT_TRUE(volumeRenderer.render(context, fields, transfer, settings));
        GTEST_LOG_(INFO) << "Pyro GPU probe: dispatch submitted transfer="
                         << static_cast<int>(transfer.kind);
        renderer.flushAndWait();
        GTEST_LOG_(INFO) << "Pyro GPU probe: GPU wait complete transfer="
                         << static_cast<int>(transfer.kind);
        auto image = renderer.readbackTextureViewToImage(volumeRenderer.outputSRV());
        GTEST_LOG_(INFO) << "Pyro GPU probe: readback complete transfer="
                         << static_cast<int>(transfer.kind);
        return image;
    };
    const QImage smoke = render(TransferFunction::makeSmoke());
    const QImage fire = render(TransferFunction::makeFire());
    const QImage explosion = render(TransferFunction::makeExplosion());
    const QImage steam = render(TransferFunction::makeSteam());
    const QImage fog = render(TransferFunction::makeFog());

    const auto visiblePixels = [](const QImage& image) {
        std::size_t count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.red() > 4 || pixel.green() > 4 || pixel.blue() > 4) {
                    ++count;
                }
            }
        }
        return count;
    };
    for (const QImage* image : {&smoke, &fire, &explosion, &steam, &fog}) {
        ASSERT_FALSE(image->isNull());
        EXPECT_EQ(image->size(), QSize(outputSize, outputSize));
        EXPECT_GT(visiblePixels(*image), 100u);
    }
    EXPECT_NE(smoke, fire);
    EXPECT_NE(smoke, explosion);
    EXPECT_NE(smoke, steam);
    EXPECT_NE(smoke, fog);
    EXPECT_NE(fire, explosion);
    EXPECT_NE(steam, fog);

    const QString outputDirectory = qEnvironmentVariable("ARTIFACT_PYRO_GPU_OUTPUT_DIR");
    if (!outputDirectory.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(outputDirectory));
        EXPECT_TRUE(smoke.save(QDir(outputDirectory).filePath(
            QStringLiteral("pyro-volume-smoke.png")), "PNG"));
        EXPECT_TRUE(fire.save(QDir(outputDirectory).filePath(
            QStringLiteral("pyro-volume-fire.png")), "PNG"));
        EXPECT_TRUE(explosion.save(QDir(outputDirectory).filePath(
            QStringLiteral("pyro-volume-explosion.png")), "PNG"));
        EXPECT_TRUE(steam.save(QDir(outputDirectory).filePath(
            QStringLiteral("pyro-volume-steam.png")), "PNG"));
        EXPECT_TRUE(fog.save(QDir(outputDirectory).filePath(
            QStringLiteral("pyro-volume-fog.png")), "PNG"));
    }
}

TEST(ClonerGpuRenderContractTest,
     PyroGpuVolumeRendererRendersSimulationFields)
{
    using namespace ArtifactCore;
    using namespace ArtifactCore::RayTrace;
    bool outputOverrideValid = false;
    const int requestedOutput = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SIM_GPU_OUTPUT_SIZE", &outputOverrideValid);
    const int outputSize = outputOverrideValid
        ? std::clamp(requestedOutput, 96, 1024) : 96;
    bool gridOverrideValid = false;
    const int requestedGrid = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SIM_GPU_GRID", &gridOverrideValid);
    const int gridSize = gridOverrideValid ? std::clamp(requestedGrid, 24, 96) : 24;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {1.2f, 1.2f, 1.2f},
        .voxelSize = 1.2f / static_cast<float>(gridSize),
        .fixedTimeStep = 1.0f / 60.0f,
        .boundaryMode = PyroBoundaryMode::Open};
    const PyroResolution resolution = domain.resolution();
    const std::array<PyroEffectKind, 5> effects{
        PyroEffectKind::Smoke, PyroEffectKind::Fire,
        PyroEffectKind::Explosion, PyroEffectKind::Steam,
        PyroEffectKind::Fog};
    const std::array<const char*, 5> effectNames{
        "smoke", "fire", "explosion", "steam", "fog"};
    const std::array<ArtifactCore::RayTrace::TransferFunction, 5> transfers{
        ArtifactCore::RayTrace::TransferFunction::makeSmoke(),
        ArtifactCore::RayTrace::TransferFunction::makeFire(),
        ArtifactCore::RayTrace::TransferFunction::makeExplosion(),
        ArtifactCore::RayTrace::TransferFunction::makeSteam(),
        ArtifactCore::RayTrace::TransferFunction::makeFog()};

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputSize, outputSize)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(renderer.device(), renderer.immediateContext()));
    const GpuVolumeRenderSettings renderSettings{
        .outputWidth = outputSize,
        .outputHeight = outputSize,
        .raySteps = 72,
        .densityScale = 1.0f,
        .temperatureScale = 1.0f,
        .opacity = 1.0f};
    auto detailSettings = renderSettings;
    detailSettings.detailStrength = 0.7f;
    detailSettings.detailScale = 12.0f;
    detailSettings.detailTimeSeconds = 0.8f;
    detailSettings.detailSeed = 2026u;

    std::array<QImage, effects.size()> images;
    std::array<QImage, effects.size()> baselineImages;
    QImage mixedImageForCapture;
    QImage extinguishedImageForCapture;
    QImage litSmokeForCapture;
    for (std::size_t index = 0; index < effects.size(); ++index) {
        auto profile = makePyroEffectProfile(effects[index], resolution);
        // Keep the widest source inside the domain so the lookdev capture
        // measures the volume instead of a source clipped by the boundary.
        profile.source.position = {0.6f, 0.6f, 0.6f};
        PyroSimulation simulation(domain);
        simulation.setSettings(profile.simulation);
        simulation.setCacheInterval(0);
        constexpr int simulationFrames = 48;
        for (int frame = 0; frame < simulationFrames; ++frame) {
            auto source = profile.source;
            source.enabled = static_cast<float>(frame) * domain.fixedTimeStep <
                             profile.sourceDurationSeconds;
            simulation.setSources(std::span<const PyroSourceState>(&source, 1));
            simulation.step(domain.fixedTimeStep);
        }

        const auto snapshot = simulation.snapshot();
        ASSERT_EQ(snapshot.resolution.width, resolution.width);
        ASSERT_EQ(snapshot.resolution.height, resolution.height);
        ASSERT_EQ(snapshot.resolution.depth, resolution.depth);
        ASSERT_FALSE(snapshot.density.empty());
        ASSERT_FALSE(snapshot.temperature.empty());
        const VolumeFieldSet fields{
            .density = {simulation.fields().densityStorage().data(),
                        {resolution.width, resolution.height, resolution.depth}},
            .temperature = {simulation.fields().temperatureStorage().data(),
                            {resolution.width, resolution.height, resolution.depth}},
            .reaction = {simulation.fields().reactionStorage().data(),
                         {resolution.width, resolution.height, resolution.depth}}};
        ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                                          transfers[index], renderSettings));
        renderer.flushAndWait();
        baselineImages[index] = renderer.readbackTextureViewToImage(
            volumeRenderer.outputSRV());
        ASSERT_FALSE(baselineImages[index].isNull());

        if (effects[index] == PyroEffectKind::Smoke) {
            // Exercise the same-device GPU texture handoff used by layer
            // compositing; the volume image itself is never read back here.
            renderer.setCanvasSize(static_cast<float>(outputSize),
                                   static_cast<float>(outputSize));
            renderer.setClearColor(ArtifactCore::FloatColor(0, 0, 0, 0));
            renderer.clear();
            renderer.drawOffscreenTexture(volumeRenderer.outputSRV(),
                QRectF(0, 0, outputSize, outputSize), 1.0f);
            renderer.flushAndWait();
            const QImage composited = renderer.readbackToImage();
            ASSERT_FALSE(composited.isNull());
            std::size_t visible = 0;
            int maximumAlphaDifference = 0;
            int maximumColorDifference = 0;
            double alphaDifferenceSum = 0.0;
            double colorDifferenceSum = 0.0;
            double premultDifferenceSum = 0.0;
            int maximumPremultDifference = 0;
            double linearPremultDifferenceSum = 0.0;
            int maximumLinearPremultDifference = 0;
            const auto expectedSrgbPremult = [](int channel, int alpha) {
                const double encoded = channel / 255.0;
                const double linear = encoded <= 0.04045 ? encoded / 12.92 :
                    std::pow((encoded + 0.055) / 1.055, 2.4);
                const double premult = linear * (alpha / 255.0);
                const double result = premult <= 0.0031308 ? 12.92 * premult :
                    1.055 * std::pow(premult, 1.0 / 2.4) - 0.055;
                return static_cast<int>(std::lround(result * 255.0));
            };
            std::size_t comparedColor = 0;
            for (int y = 0; y < composited.height(); ++y) {
                for (int x = 0; x < composited.width(); ++x) {
                    const QColor result = composited.pixelColor(x, y);
                    const QColor source = baselineImages[index].pixelColor(x, y);
                    visible += result.alpha() > 8 ? 1u : 0u;
                    const int alphaDifference = std::abs(result.alpha() - source.alpha());
                    maximumAlphaDifference = std::max(maximumAlphaDifference, alphaDifference);
                    alphaDifferenceSum += alphaDifference;
                    if (source.alpha() > 32 && result.alpha() > 32) {
                        const int colorDifference = std::max({std::abs(result.red() - source.red()),
                            std::abs(result.green() - source.green()), std::abs(result.blue() - source.blue())});
                        maximumColorDifference = std::max(maximumColorDifference, colorDifference);
                        colorDifferenceSum += colorDifference;
                        const int premultDifference = std::max({
                            std::abs(result.red() - (source.red() * source.alpha() + 127) / 255),
                            std::abs(result.green() - (source.green() * source.alpha() + 127) / 255),
                            std::abs(result.blue() - (source.blue() * source.alpha() + 127) / 255)});
                        maximumPremultDifference = std::max(maximumPremultDifference, premultDifference);
                        premultDifferenceSum += premultDifference;
                        const int linearDifference = std::max({
                            std::abs(result.red() - expectedSrgbPremult(source.red(), source.alpha())),
                            std::abs(result.green() - expectedSrgbPremult(source.green(), source.alpha())),
                            std::abs(result.blue() - expectedSrgbPremult(source.blue(), source.alpha()))});
                        maximumLinearPremultDifference = std::max(maximumLinearPremultDifference, linearDifference);
                        linearPremultDifferenceSum += linearDifference;
                        ++comparedColor;
                    }
                }
            }
            EXPECT_GT(visible, 0u);
            EXPECT_EQ(composited.pixelColor(0, 0).alpha(), 0);
            GTEST_LOG_(INFO) << "Pyro texture handoff alpha max/mean="
                << maximumAlphaDifference << '/'
                << alphaDifferenceSum / (composited.width() * composited.height())
                << " color max/mean=" << maximumColorDifference << '/'
                << (comparedColor ? colorDifferenceSum / comparedColor : 0.0)
                << " premult max/mean=" << maximumPremultDifference << '/'
                << (comparedColor ? premultDifferenceSum / comparedColor : 0.0)
                << " linear-premult max/mean=" << maximumLinearPremultDifference << '/'
                << (comparedColor ? linearPremultDifferenceSum / comparedColor : 0.0)
                << " color samples=" << comparedColor;

            renderer.setClearColor(ArtifactCore::FloatColor(0.08f, 0.22f, 0.45f, 1.0f));
            renderer.clear();
            renderer.flushAndWait();
            const QImage background = renderer.readbackToImage();
            ASSERT_FALSE(background.isNull());
            renderer.drawOffscreenTexture(volumeRenderer.outputSRV(),
                QRectF(0, 0, outputSize, outputSize), 1.0f);
            renderer.flushAndWait();
            const QImage overBackground = renderer.readbackToImage();
            ASSERT_FALSE(overBackground.isNull());
            const auto decodeSrgb = [](int value) {
                const double c = value / 255.0;
                return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
            };
            const auto encodeSrgb = [](double value) {
                const double c = std::clamp(value, 0.0, 1.0);
                return static_cast<int>(std::lround(255.0 * (c <= 0.0031308 ?
                    12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055)));
            };
            int maxOverDifference = 0;
            double meanOverDifference = 0.0;
            std::size_t overSamples = 0;
            for (int y = 0; y < overBackground.height(); ++y) {
                for (int x = 0; x < overBackground.width(); ++x) {
                    const QColor source = baselineImages[index].pixelColor(x, y);
                    if (source.alpha() <= 32) continue;
                    const QColor dest = background.pixelColor(x, y);
                    const QColor result = overBackground.pixelColor(x, y);
                    const double alpha = source.alpha() / 255.0;
                    const auto expected = [&](int src, int dst) {
                        return encodeSrgb(decodeSrgb(src) * alpha + decodeSrgb(dst) * (1.0 - alpha));
                    };
                    const int difference = std::max({
                        std::abs(result.red() - expected(source.red(), dest.red())),
                        std::abs(result.green() - expected(source.green(), dest.green())),
                        std::abs(result.blue() - expected(source.blue(), dest.blue()))});
                    maxOverDifference = std::max(maxOverDifference, difference);
                    meanOverDifference += difference;
                    ++overSamples;
                }
            }
            ASSERT_GT(overSamples, 0u);
            GTEST_LOG_(INFO) << "Pyro over background max/mean=" << maxOverDifference
                << '/' << meanOverDifference / overSamples << " samples=" << overSamples;

            renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
            renderer.clear();
            renderer.drawOffscreenTexture(volumeRenderer.outputSRV(),
                QRectF(0, 0, outputSize, outputSize), 0.5f);
            renderer.flushAndWait();
            const QImage halfOpacity = renderer.readbackToImage();
            ASSERT_FALSE(halfOpacity.isNull());
            int maxOpacityError = 0;
            for (int y = 0; y < halfOpacity.height(); ++y) {
                for (int x = 0; x < halfOpacity.width(); ++x) {
                    const int expectedAlpha =
                        (baselineImages[index].pixelColor(x, y).alpha() + 1) / 2;
                    maxOpacityError = std::max(maxOpacityError,
                        std::abs(halfOpacity.pixelColor(x, y).alpha() - expectedAlpha));
                }
            }
            EXPECT_LE(maxOpacityError, 1);

            renderer.clear();
            renderer.drawOffscreenTexture(volumeRenderer.outputSRV(),
                QRectF(0, 0, outputSize, outputSize), 1.0f);
            renderer.drawSolidRect(0.0f, 0.0f, static_cast<float>(outputSize),
                static_cast<float>(outputSize), ArtifactCore::FloatColor(1, 0, 0, 1));
            renderer.flushAndWait();
            const QImage foreground = renderer.readbackToImage();
            ASSERT_FALSE(foreground.isNull());
            const QColor foregroundCenter = foreground.pixelColor(outputSize / 2, outputSize / 2);
            EXPECT_GT(foregroundCenter.red(), 250);
            EXPECT_LT(foregroundCenter.green(), 3);
            EXPECT_LT(foregroundCenter.blue(), 3);
            EXPECT_EQ(foregroundCenter.alpha(), 255);

            GpuVolumeRenderer secondVolume;
            ASSERT_TRUE(secondVolume.initialize(renderer.device(), renderer.immediateContext()));
            ASSERT_TRUE(secondVolume.render(renderer.immediateContext(), fields,
                ArtifactCore::RayTrace::TransferFunction::makeFog(), renderSettings));
            renderer.clear();
            renderer.drawOffscreenTexture(volumeRenderer.outputSRV(),
                QRectF(0, 0, outputSize / 2.0, outputSize), 1.0f);
            renderer.drawOffscreenTexture(secondVolume.outputSRV(),
                QRectF(outputSize / 2.0, 0, outputSize / 2.0, outputSize), 1.0f);
            renderer.flushAndWait();
            const QImage twoVolumes = renderer.readbackToImage();
            ASSERT_FALSE(twoVolumes.isNull());
            std::size_t leftVisible = 0, rightVisible = 0;
            for (int y = 0; y < outputSize; ++y) {
                for (int x = 0; x < outputSize; ++x) {
                    if (twoVolumes.pixelColor(x, y).alpha() <= 8) continue;
                    if (x < outputSize / 2) ++leftVisible;
                    else ++rightVisible;
                }
            }
            EXPECT_GT(leftVisible, 0u);
            EXPECT_GT(rightVisible, 0u);

            // Diagnostic: overwriting a texture before the queued sprite is
            // submitted may change which volume the sprite actually samples.
            renderer.clear();
            renderer.drawOffscreenTexture(volumeRenderer.outputSRV(),
                QRectF(0, 0, outputSize, outputSize), 1.0f);
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                ArtifactCore::RayTrace::TransferFunction::makeFog(), renderSettings));
            renderer.flushAndWait();
            const QImage overwritten = renderer.readbackToImage();
            ASSERT_FALSE(overwritten.isNull());
            GTEST_LOG_(INFO) << "Pyro queued texture overwrite preserves smoke="
                             << (overwritten == composited);
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                ArtifactCore::RayTrace::TransferFunction::makeSmoke(), renderSettings));
            renderer.clear();
            renderer.drawOffscreenTexture(volumeRenderer.outputSRV(),
                QRectF(0, 0, outputSize, outputSize), 1.0f);
            renderer.flush();
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                ArtifactCore::RayTrace::TransferFunction::makeFog(), renderSettings));
            renderer.flushAndWait();
            const QImage submittedBeforeOverwrite = renderer.readbackToImage();
            ASSERT_FALSE(submittedBeforeOverwrite.isNull());
            EXPECT_EQ(submittedBeforeOverwrite, composited)
                << "the queued sprite must be submitted before its texture is reused";
        }

        if (effects[index] == PyroEffectKind::Fire) {
            const VolumeFieldSet temperatureOnlyFields{
                .density = fields.density,
                .temperature = fields.temperature};
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(),
                                              temperatureOnlyFields,
                                              transfers[index], renderSettings));
            renderer.flushAndWait();
            const auto temperatureOnlyImage = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            EXPECT_NE(baselineImages[index], temperatureOnlyImage)
                << "the reaction field should limit flame emission to burning cells";

            const auto mixedTransfer =
                ArtifactCore::RayTrace::TransferFunction::makeFireAndSmoke();
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                                              mixedTransfer, renderSettings));
            renderer.flushAndWait();
            const auto mixedImage = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            ASSERT_FALSE(mixedImage.isNull());
            mixedImageForCapture = mixedImage;
            EXPECT_NE(mixedImage, baselineImages[index])
                << "the combined transfer should render smoke scattering with flame emission";
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                ArtifactCore::RayTrace::TransferFunction::makeSmoke(), renderSettings));
            renderer.flushAndWait();
            const auto smokeFromFireFields = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            ASSERT_FALSE(smokeFromFireFields.isNull());
            EXPECT_NE(mixedImage, smokeFromFireFields)
                << "the combined transfer should preserve a distinct fire component";

            const auto cellCount = static_cast<std::size_t>(resolution.width) *
                                   static_cast<std::size_t>(resolution.height) *
                                   static_cast<std::size_t>(resolution.depth);
            std::vector<float> noReaction(cellCount, 0.0f);
            const VolumeFieldSet extinguishedFields{
                .density = fields.density,
                .temperature = fields.temperature,
                .reaction = {noReaction.data(), fields.density.resolution}};
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(),
                                              extinguishedFields, mixedTransfer,
                                              renderSettings));
            renderer.flushAndWait();
            const auto smokeAfterExtinguish = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            ASSERT_FALSE(smokeAfterExtinguish.isNull());
            extinguishedImageForCapture = smokeAfterExtinguish;
            std::size_t visibleAfterExtinguish = 0;
            for (int y = 0; y < smokeAfterExtinguish.height(); ++y) {
                for (int x = 0; x < smokeAfterExtinguish.width(); ++x) {
                    if (smokeAfterExtinguish.pixelColor(x, y).alpha() > 8) {
                        ++visibleAfterExtinguish;
                    }
                }
            }
            EXPECT_GT(visibleAfterExtinguish, 0u)
                << "zero combustion reaction should leave smoke visible after flames extinguish";
        }

        ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                                          transfers[index], detailSettings));
        renderer.flushAndWait();
        images[index] = renderer.readbackTextureViewToImage(
            volumeRenderer.outputSRV());
        ASSERT_FALSE(images[index].isNull());
        EXPECT_EQ(images[index].size(), QSize(outputSize, outputSize));
        std::size_t visibleBorderPixels = 0;
        for (int pixel = 0; pixel < outputSize; ++pixel) {
            visibleBorderPixels += images[index].pixelColor(pixel, 0).alpha() > 8;
            visibleBorderPixels += images[index].pixelColor(pixel, outputSize - 1).alpha() > 8;
            visibleBorderPixels += images[index].pixelColor(0, pixel).alpha() > 8;
            visibleBorderPixels += images[index].pixelColor(outputSize - 1, pixel).alpha() > 8;
        }
        EXPECT_EQ(visibleBorderPixels, 0u)
            << effectNames[index] << " should fit inside the lookdev frame";

        if (index == 0) {
            auto lightingSettings = detailSettings;
            lightingSettings.smokeLightingStrength = 1.0f;
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                                              transfers[index], lightingSettings));
            renderer.flushAndWait();
            litSmokeForCapture = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            ASSERT_FALSE(litSmokeForCapture.isNull());
            EXPECT_NE(litSmokeForCapture, images[index])
                << "directional smoke lighting should change RGB";
            std::size_t changedAlpha = 0;
            for (int y = 0; y < outputSize; ++y) {
                for (int x = 0; x < outputSize; ++x) {
                    changedAlpha += litSmokeForCapture.pixelColor(x, y).alpha() !=
                                    images[index].pixelColor(x, y).alpha();
                }
            }
            EXPECT_EQ(changedAlpha, 0u) << "lighting must not change opacity";
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                                              transfers[index], detailSettings));
            renderer.flushAndWait();
            const auto repeated = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            EXPECT_EQ(images[index], repeated)
                << "the same detail seed and time should be deterministic";

            auto laterSettings = detailSettings;
            laterSettings.detailTimeSeconds += 0.5f;
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                                              transfers[index], laterSettings));
            renderer.flushAndWait();
            const auto later = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            EXPECT_NE(images[index], later)
                << "detail should evolve when its absolute time changes";
        }
    }

    for (std::size_t first = 0; first < images.size(); ++first) {
        EXPECT_NE(images[first], QImage{});
        for (std::size_t second = first + 1; second < images.size(); ++second) {
            EXPECT_NE(images[first], images[second])
                << "effect profiles should remain visually distinct when rendered "
                   "from actual PyroSimulation fields";
        }
    }
    for (const std::size_t index : {0u, 1u, 2u}) {
        EXPECT_NE(images[index], baselineImages[index])
            << "detail shaping should change the smoke, fire, and explosion looks";
    }

    const QString outputDirectory =
        qEnvironmentVariable("ARTIFACT_PYRO_SIM_GPU_OUTPUT_DIR");
    if (!outputDirectory.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(outputDirectory));
        EXPECT_TRUE(mixedImageForCapture.save(
            QDir(outputDirectory).filePath(QStringLiteral("pyro-simulation-fire-smoke.png")), "PNG"));
        EXPECT_TRUE(extinguishedImageForCapture.save(
            QDir(outputDirectory).filePath(QStringLiteral("pyro-simulation-extinguished-smoke.png")), "PNG"));
        EXPECT_TRUE(litSmokeForCapture.save(
            QDir(outputDirectory).filePath(QStringLiteral("pyro-simulation-smoke-lit.png")), "PNG"));
        for (std::size_t index = 0; index < images.size(); ++index) {
            EXPECT_TRUE(images[index].save(
                QDir(outputDirectory).filePath(
                    QStringLiteral("pyro-simulation-%1.png")
                        .arg(QString::fromLatin1(effectNames[index]))),
                "PNG"));
        }
    }
}

TEST(ClonerGpuRenderContractTest,
     PyroFireAndSmokeShareOneSimulationVolume)
{
    using namespace ArtifactCore;
    using namespace ArtifactCore::RayTrace;
    constexpr int gridSize = 24;
    constexpr int outputSize = 160;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {1.2f, 1.2f, 1.2f},
        .voxelSize = 1.2f / static_cast<float>(gridSize),
        .fixedTimeStep = 1.0 / 60.0,
        .boundaryMode = PyroBoundaryMode::Open};
    const auto profile = makePyroEffectProfile(
        PyroEffectKind::Fire, domain.resolution());
    PyroSimulation simulation(domain);
    simulation.setSettings(profile.simulation);
    simulation.setCacheInterval(0);
    auto source = profile.source;
    source.position = {0.6f, 0.6f, 0.6f};
    for (int frame = 0; frame < 48; ++frame) {
        source.enabled = static_cast<float>(frame) * domain.fixedTimeStep <
                         profile.sourceDurationSeconds;
        simulation.setSources(std::span<const PyroSourceState>(&source, 1));
        simulation.step(domain.fixedTimeStep);
    }

    const auto resolution = simulation.fields().resolution();
    const VolumeResolution volumeResolution{
        resolution.width, resolution.height, resolution.depth};
    const VolumeFieldSet fields{
        .density = {simulation.fields().densityStorage().data(), volumeResolution},
        .temperature = {simulation.fields().temperatureStorage().data(), volumeResolution},
        .reaction = {simulation.fields().reactionStorage().data(), volumeResolution}};
    ASSERT_GT(std::accumulate(simulation.fields().densityStorage().begin(),
                             simulation.fields().densityStorage().end(), 0.0), 0.0);
    ASSERT_GT(std::accumulate(simulation.fields().reactionStorage().begin(),
                             simulation.fields().reactionStorage().end(), 0.0), 0.0);

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputSize, outputSize)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(renderer.device(), renderer.immediateContext()));
    const GpuVolumeRenderSettings settings{
        .outputWidth = outputSize,
        .outputHeight = outputSize,
        .raySteps = 72,
        .densityScale = 1.0f,
        .temperatureScale = 1.0f,
        .opacity = 1.0f};
    ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
        TransferFunction::makeFireAndSmoke(), settings));
    renderer.flushAndWait();
    const QImage combined = renderer.readbackTextureViewToImage(
        volumeRenderer.outputSRV());
    ASSERT_FALSE(combined.isNull());

    ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
        TransferFunction::makeSmoke(), settings));
    renderer.flushAndWait();
    const QImage smokeOnly = renderer.readbackTextureViewToImage(
        volumeRenderer.outputSRV());
    ASSERT_FALSE(smokeOnly.isNull());
    EXPECT_NE(combined, smokeOnly)
        << "one shared Pyro field should preserve fire emission and smoke density";

    const QString outputDirectory =
        qEnvironmentVariable("ARTIFACT_PYRO_FIRE_SMOKE_OUTPUT_DIR");
    if (!outputDirectory.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(outputDirectory));
        EXPECT_TRUE(combined.save(QDir(outputDirectory).filePath(
            QStringLiteral("pyro-fire-and-smoke.png")), "PNG"));
        EXPECT_TRUE(smokeOnly.save(QDir(outputDirectory).filePath(
            QStringLiteral("pyro-smoke-only.png")), "PNG"));
    }
}

TEST(ClonerGpuRenderContractTest,
     PyroSmokeDistributionSweepReportsResolutionAndVerticalSpread)
{
    using namespace ArtifactCore;
    using namespace ArtifactCore::RayTrace;
    constexpr int gridSize = 24;
    constexpr int outputSize = 192;
    const PyroDomain domain{
        .min = {-0.6f, -0.6f, -0.6f},
        .max = {0.6f, 0.6f, 0.6f},
        .voxelSize = 1.2f / static_cast<float>(gridSize),
        .fixedTimeStep = 1.0 / 60.0,
        .boundaryMode = PyroBoundaryMode::Open};
    struct Condition {
        const char* name;
        float sourceY;
        float buoyancy;
        float smokeTurbulence;
        float turbulenceScale;
    };
    const std::array<Condition, 4> conditions{{
        {"baseline", -0.36f, 0.5f, 0.0f, 18.0f},
        {"low_buoyancy", -0.36f, 0.15f, 0.0f, 18.0f},
        {"lower_source", -0.48f, 0.5f, 0.0f, 18.0f},
        {"low_buoyancy_turbulence", -0.48f, 0.15f, 2.0f, 10.0f}}};

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputSize, outputSize)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(renderer.device(), renderer.immediateContext()));
    const GpuVolumeRenderSettings renderSettings{
        .outputWidth = outputSize,
        .outputHeight = outputSize,
        .raySteps = 72,
        .densityScale = 1.0f,
        .temperatureScale = 1.0f,
        .opacity = 1.0f};
    const QString outputDirectory =
        qEnvironmentVariable("ARTIFACT_PYRO_DISTRIBUTION_OUTPUT_DIR");
    if (!outputDirectory.isEmpty())
        ASSERT_TRUE(QDir().mkpath(outputDirectory));

    for (const auto& condition : conditions) {
        auto profile = makePyroEffectProfile(
            PyroEffectKind::Smoke, domain.resolution());
        profile.simulation.buoyancy = condition.buoyancy;
        profile.simulation.smokeTurbulence = condition.smokeTurbulence;
        profile.simulation.smokeTurbulenceScale = condition.turbulenceScale;
        profile.simulation.pressureIterations = 6.0f;
        profile.source.position = {0.0f, condition.sourceY, 0.0f};

        PyroSimulation simulation(domain);
        simulation.setSettings(profile.simulation);
        simulation.setCacheInterval(0);
        const int emissionFrames = static_cast<int>(std::ceil(
            profile.sourceDurationSeconds / domain.fixedTimeStep));
        auto source = profile.source;
        source.position = {0.0f, condition.sourceY, 0.0f};
        simulation.setSources(std::span<const PyroSourceState>(&source, 1));
        for (int frame = 0; frame < emissionFrames + 24; ++frame) {
            source.enabled = static_cast<float>(frame) * domain.fixedTimeStep <
                             profile.sourceDurationSeconds;
            simulation.setSources(
                std::span<const PyroSourceState>(&source, 1));
            simulation.step(domain.fixedTimeStep);
        }

        const auto resolution = simulation.fields().resolution();
        const std::size_t cellCount = resolution.cellCount();
        const auto& density = simulation.fields().densityStorage();
        const auto& temperature = simulation.fields().temperatureStorage();
        const auto& reaction = simulation.fields().reactionStorage();
        ASSERT_EQ(density.size(), cellCount);
        double mass = 0.0;
        double momentY = 0.0;
        double upperThirdMass = 0.0;
        double lowerThirdMass = 0.0;
        for (int z = 0; z < resolution.depth; ++z)
            for (int y = 0; y < resolution.height; ++y)
                for (int x = 0; x < resolution.width; ++x) {
                    const auto index = static_cast<std::size_t>(x) +
                        resolution.width * (static_cast<std::size_t>(y) +
                        resolution.height * static_cast<std::size_t>(z));
                    const double value = std::max(0.0f, density[index]);
                    mass += value;
                    momentY += value * (static_cast<double>(y) + 0.5);
                    if (y < resolution.height / 3) lowerThirdMass += value;
                    if (y >= (2 * resolution.height) / 3) upperThirdMass += value;
                }
        ASSERT_GT(mass, 0.0) << condition.name;
        const VolumeResolution volumeResolution{
            resolution.width, resolution.height, resolution.depth};
        const VolumeFieldSet fields{
            .density = {const_cast<float*>(density.data()), volumeResolution},
            .temperature = {const_cast<float*>(temperature.data()), volumeResolution},
            .reaction = {const_cast<float*>(reaction.data()), volumeResolution}};
        ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
            TransferFunction::makeSmoke(), renderSettings));
        renderer.flushAndWait();
        const QImage image = renderer.readbackTextureViewToImage(
            volumeRenderer.outputSRV());
        ASSERT_FALSE(image.isNull());
        double alphaMass = 0.0;
        double alphaMomentY = 0.0;
        std::array<double, 3> alphaThirdMass{};
        double alphaWeightedLumaMass = 0.0;
        double alphaWeightedLumaMomentY = 0.0;
        std::array<double, 3> alphaWeightedLumaThirdMass{};
        std::array<std::size_t, 3> visiblePixelsByThird{};
        for (int y = 0; y < image.height(); ++y) {
            const std::size_t third = std::min<std::size_t>(
                2, 3 * static_cast<std::size_t>(y) /
                    static_cast<std::size_t>(image.height()));
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                const float alpha = pixel.alphaF();
                const double alphaWeightedLuma = alpha * (
                    0.2126 * pixel.redF() + 0.7152 * pixel.greenF() +
                    0.0722 * pixel.blueF());
                alphaMass += alpha;
                // QImage rows increase downward; report the same world-up
                // convention as densityCentroidY01 (bottom=0, top=1).
                alphaMomentY += alpha *
                    (static_cast<double>(outputSize) - y - 0.5);
                alphaThirdMass[third] += alpha;
                alphaWeightedLumaMass += alphaWeightedLuma;
                alphaWeightedLumaMomentY += alphaWeightedLuma *
                    (static_cast<double>(image.height()) - y - 0.5);
                alphaWeightedLumaThirdMass[third] += alphaWeightedLuma;
                if (alpha > 0.04f) ++visiblePixelsByThird[third];
            }
        }
        ASSERT_GT(alphaMass, 0.0) << condition.name;
        ASSERT_GT(alphaWeightedLumaMass, 0.0) << condition.name;
        GTEST_LOG_(INFO) << "Pyro smoke distribution condition=" << condition.name
            << " mass=" << mass
            << " centroidY01=" << momentY / mass / resolution.height
            << " upperThird=" << upperThirdMass / mass
            << " lowerThird=" << lowerThirdMass / mass
            << " outputAlphaCentroidY01="
            << alphaMomentY / alphaMass / image.height()
            << " outputAlphaThirdShares(top/middle/bottom)="
            << alphaThirdMass[0] / alphaMass << '/'
            << alphaThirdMass[1] / alphaMass << '/'
            << alphaThirdMass[2] / alphaMass
            << " alphaWeightedLumaCentroidY01="
            << alphaWeightedLumaMomentY / alphaWeightedLumaMass /
                image.height()
            << " alphaWeightedLumaThirdShares(top/middle/bottom)="
            << alphaWeightedLumaThirdMass[0] / alphaWeightedLumaMass << '/'
            << alphaWeightedLumaThirdMass[1] / alphaWeightedLumaMass << '/'
            << alphaWeightedLumaThirdMass[2] / alphaWeightedLumaMass
            << " visiblePixels(top/middle/bottom)="
            << visiblePixelsByThird[0] << '/' << visiblePixelsByThird[1]
            << '/' << visiblePixelsByThird[2]
            << " grid=" << resolution.width << 'x' << resolution.height
            << 'x' << resolution.depth
            << " buoyancy=" << condition.buoyancy
            << " turbulence=" << condition.smokeTurbulence
            << " scale=" << condition.turbulenceScale;
        if (!outputDirectory.isEmpty()) {
            EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(
                QStringLiteral("pyro-smoke-%1.png")
                    .arg(QString::fromLatin1(condition.name))), "PNG"));
        }
    }
}

TEST(ClonerGpuRenderContractTest,
     PyroVolumeRenderResolutionSweepUsesTheSameSimulationField)
{
    using namespace ArtifactCore;
    using namespace ArtifactCore::RayTrace;
    constexpr int gridSize = 24;
    const std::array<int, 3> outputSizes{128, 256, 512};
    const PyroDomain domain{
        .min = {-0.6f, -0.6f, -0.6f},
        .max = {0.6f, 0.6f, 0.6f},
        .voxelSize = 1.2f / static_cast<float>(gridSize),
        .fixedTimeStep = 1.0 / 60.0,
        .boundaryMode = PyroBoundaryMode::Open};
    auto profile = makePyroEffectProfile(PyroEffectKind::Smoke,
                                         domain.resolution());
    profile.simulation.pressureIterations = 6.0f;
    profile.source.position = {0.0f, -0.36f, 0.0f};
    PyroSimulation simulation(domain);
    simulation.setSettings(profile.simulation);
    simulation.setCacheInterval(0);
    auto source = profile.source;
    simulation.setSources(std::span<const PyroSourceState>(&source, 1));
    const int emissionFrames = static_cast<int>(std::ceil(
        profile.sourceDurationSeconds / domain.fixedTimeStep));
    for (int frame = 0; frame < emissionFrames + 24; ++frame) {
        source.enabled = static_cast<float>(frame) * domain.fixedTimeStep <
                         profile.sourceDurationSeconds;
        simulation.setSources(std::span<const PyroSourceState>(&source, 1));
        simulation.step(domain.fixedTimeStep);
    }

    const auto resolution = simulation.fields().resolution();
    const VolumeResolution volumeResolution{
        resolution.width, resolution.height, resolution.depth};
    const auto& density = simulation.fields().densityStorage();
    const auto& temperature = simulation.fields().temperatureStorage();
    const auto& reaction = simulation.fields().reactionStorage();
    const VolumeFieldSet fields{
        .density = {const_cast<float*>(density.data()), volumeResolution},
        .temperature = {const_cast<float*>(temperature.data()), volumeResolution},
        .reaction = {const_cast<float*>(reaction.data()), volumeResolution}};

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputSizes.back(), outputSizes.back())) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(renderer.device(), renderer.immediateContext()));
    const QString outputDirectory =
        qEnvironmentVariable("ARTIFACT_PYRO_RESOLUTION_OUTPUT_DIR");
    if (!outputDirectory.isEmpty())
        ASSERT_TRUE(QDir().mkpath(outputDirectory));

    for (const int outputSize : outputSizes) {
        GpuVolumeRenderSettings settings;
        settings.outputWidth = outputSize;
        settings.outputHeight = outputSize;
        settings.raySteps = 72;
        settings.densityScale = 1.0f;
        settings.temperatureScale = 1.0f;
        settings.opacity = 1.0f;
        ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
            TransferFunction::makeSmoke(), settings));
        renderer.flushAndWait();
        const QImage image = renderer.readbackTextureViewToImage(
            volumeRenderer.outputSRV());
        ASSERT_FALSE(image.isNull());
        ASSERT_EQ(image.size(), QSize(outputSize, outputSize));

        double alphaSum = 0.0;
        double alphaMomentY = 0.0;
        std::size_t visiblePixels = 0;
        double gradientEnergy = 0.0;
        for (int y = 0; y < outputSize; ++y) {
            for (int x = 0; x < outputSize; ++x) {
                const float alpha = image.pixelColor(x, y).alphaF();
                alphaSum += alpha;
                alphaMomentY += alpha * (static_cast<double>(y) + 0.5);
                visiblePixels += alpha > 0.04f;
                if (x + 1 < outputSize) {
                    const float next = image.pixelColor(x + 1, y).alphaF();
                    gradientEnergy += std::abs(next - alpha);
                }
                if (y + 1 < outputSize) {
                    const float next = image.pixelColor(x, y + 1).alphaF();
                    gradientEnergy += std::abs(next - alpha);
                }
            }
        }
        ASSERT_GT(alphaSum, 0.0) << outputSize;
        GTEST_LOG_(INFO) << "Pyro same-field render resolution=" << outputSize
            << "x" << outputSize << " solver=" << resolution.width << "x"
            << resolution.height << "x" << resolution.depth
            << " alphaMass=" << alphaSum
            << " centroidY01=" << alphaMomentY / alphaSum / outputSize
            << " visiblePixels=" << visiblePixels
            << " normalizedGradient=" << gradientEnergy / alphaSum;
        if (!outputDirectory.isEmpty()) {
            EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(
                QStringLiteral("pyro-smoke-render-%1.png").arg(outputSize)),
                "PNG"));
        }
    }
}

TEST(ClonerGpuRenderContractTest,
     PyroSmokeSolverResolutionSweepUsesMatchedPhysicalSetup)
{
    using namespace ArtifactCore;
    using namespace ArtifactCore::RayTrace;
    constexpr std::array<int, 2> gridSizes{24, 48};
    struct Condition {
        const char* name;
        float sourceVelocityScale;
        float buoyancyScale;
        float turbulenceStrength;
    };
    constexpr std::array<Condition, 4> conditions{
        {{"baseline", 1.0f, 1.0f, 0.0f},
         {"low-buoyancy", 1.0f, 0.5f, 0.0f},
         {"slow-source", 0.5f, 1.0f, 0.0f},
         {"turbulent", 1.0f, 1.0f, 1.0f}}};
    constexpr int simulationFrames = 48;
    constexpr int emissionFrames = 24;
    constexpr int outputSize = 384;
    const QString outputDirectory =
        qEnvironmentVariable("ARTIFACT_PYRO_SOLVER_RESOLUTION_OUTPUT_DIR");
    if (!outputDirectory.isEmpty())
      ASSERT_TRUE(QDir().mkpath(outputDirectory));

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputSize, outputSize)) {
      GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(renderer.device(),
                                          renderer.immediateContext()));

    for (const auto &condition : conditions) {
      for (const int gridSize : gridSizes) {
        const int substepsPerFrame = gridSize / gridSizes.front();
        const int simulationSteps = simulationFrames * substepsPerFrame;
        const int sourceEmissionSteps = emissionFrames * substepsPerFrame;
        const double solverStepSeconds =
            1.0 / (60.0 * static_cast<double>(substepsPerFrame));
        const float voxelSize = 1.2f / static_cast<float>(gridSize);
        const PyroDomain domain{.min = {-0.6f, -0.6f, -0.6f},
                                .max = {0.6f, 0.6f, 0.6f},
                                .voxelSize = voxelSize,
                                .fixedTimeStep = solverStepSeconds,
                                .boundaryMode = PyroBoundaryMode::Open};
        auto profile =
            makePyroEffectProfile(PyroEffectKind::Smoke, domain.resolution());
        // Match the measured post-projection divergence across grids. The
        // existing 3D pressure sweep found that 8 iterations diverged more
        // than 3x at 48^3; 36 iterations brought it close to the 24^3 result.
        profile.simulation.pressureIterations = gridSize == 24 ? 8.0f : 36.0f;
        // Pyro buoyancy is added to grid-cell velocity. Scale acceleration
        // with cells per world unit so both domains receive the same physical
        // upward acceleration, just as the effect profile scales source speed.
        profile.simulation.buoyancy *= static_cast<float>(gridSize) / 24.0f;
        profile.simulation.buoyancy *= condition.buoyancyScale;
        profile.simulation.smokeTurbulence =
            condition.turbulenceStrength * static_cast<float>(gridSize) / 24.0f;
        profile.simulation.smokeTurbulenceScale =
            static_cast<float>(gridSize) * 0.3f;
        profile.source.velocity.x *= condition.sourceVelocityScale;
        profile.source.velocity.y *= condition.sourceVelocityScale;
        profile.source.velocity.z *= condition.sourceVelocityScale;
        profile.source.position = {0.0f, -0.36f, 0.0f};

        PyroSimulation simulation(domain);
        simulation.setSettings(profile.simulation);
        simulation.setCacheInterval(0);
        auto source = profile.source;
        source.position = {0.0f, -0.36f, 0.0f};
        simulation.setSources(std::span<const PyroSourceState>(&source, 1));
        const auto simulationStart = std::chrono::steady_clock::now();
        for (int frame = 0; frame < simulationSteps; ++frame) {
          source.enabled = frame < sourceEmissionSteps;
          // setSources copies the source state; update the solver-owned
          // source after changing the local emission flag.
          simulation.setSources(std::span<const PyroSourceState>(&source, 1));
          simulation.step(solverStepSeconds);
        }
        const double simulationMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - simulationStart)
                .count();

        const auto resolution = simulation.fields().resolution();
        const VolumeResolution volumeResolution{
            resolution.width, resolution.height, resolution.depth};
        const auto &density = simulation.fields().densityStorage();
        const auto &temperature = simulation.fields().temperatureStorage();
        const auto &reaction = simulation.fields().reactionStorage();
        const auto &solverFields = simulation.fields();
        const auto velocityView = solverFields.velocityView();
        ASSERT_EQ(density.size(), resolution.cellCount());
        double physicalDensityMass = 0.0;
        double densityMomentY = 0.0;
        double densityMinY = 1.0;
        double densityMaxY = 0.0;
        double upperThirdDensityMass = 0.0;
        double middleThirdDensityMass = 0.0;
        double lowerThirdDensityMass = 0.0;
        std::array<double, 6> boundaryDensityMass{};
        double divergenceSquaredSum = 0.0;
        std::size_t divergenceSamples = 0;
        for (int z = 0; z < resolution.depth; ++z) {
          for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
              const auto index =
                  static_cast<std::size_t>(x) +
                  static_cast<std::size_t>(resolution.width) *
                      (static_cast<std::size_t>(y) +
                       static_cast<std::size_t>(resolution.height) *
                           static_cast<std::size_t>(z));
              const double value = std::max(0.0f, density[index]);
              const double cellMass = value * voxelSize * voxelSize * voxelSize;
              physicalDensityMass += cellMass;
              densityMomentY += cellMass * (y + 0.5);
              if (value > 1.0e-4) {
                const double normalizedY =
                    (y + 0.5) / static_cast<double>(resolution.height);
                densityMinY = std::min(densityMinY, normalizedY);
                densityMaxY = std::max(densityMaxY, normalizedY);
              }
              if (y >= (2 * resolution.height) / 3) {
                upperThirdDensityMass += cellMass;
              } else if (y >= resolution.height / 3) {
                middleThirdDensityMass += cellMass;
              } else {
                lowerThirdDensityMass += cellMass;
              }
              const int boundaryBand = std::max(1, resolution.width / 24);
              if (x < boundaryBand)
                boundaryDensityMass[0] += cellMass;
              if (x >= resolution.width - boundaryBand)
                boundaryDensityMass[1] += cellMass;
              if (y < boundaryBand)
                boundaryDensityMass[2] += cellMass;
              if (y >= resolution.height - boundaryBand)
                boundaryDensityMass[3] += cellMass;
              if (z < boundaryBand)
                boundaryDensityMass[4] += cellMass;
              if (z >= resolution.depth - boundaryBand)
                boundaryDensityMass[5] += cellMass;
              if (x > 0 && x + 1 < resolution.width && y > 0 &&
                  y + 1 < resolution.height && z > 0 &&
                  z + 1 < resolution.depth) {
                const float divergence = solverFields.divergence(
                    velocityView,
                    {static_cast<float>(x), static_cast<float>(y),
                     static_cast<float>(z)},
                    domain.boundaryMode);
                divergenceSquaredSum += divergence * divergence;
                ++divergenceSamples;
              }
            }
          }
        }
        ASSERT_GT(physicalDensityMass, 0.0);
        ASSERT_GT(divergenceSamples, 0u);

        const VolumeFieldSet fields{
            .density = {const_cast<float *>(density.data()), volumeResolution},
            .temperature = {const_cast<float *>(temperature.data()),
                            volumeResolution},
            .reaction = {const_cast<float *>(reaction.data()),
                         volumeResolution}};
        GpuVolumeRenderSettings renderSettings;
        renderSettings.outputWidth = outputSize;
        renderSettings.outputHeight = outputSize;
        // Sample at least once per solver cell along the ray so a solver
        // resolution change is not hidden by a fixed, coarser renderer step.
        renderSettings.raySteps = gridSize * 2;
        renderSettings.densityScale = 1.0f;
        renderSettings.temperatureScale = 1.0f;
        renderSettings.opacity = 1.0f;
        const auto renderStart = std::chrono::steady_clock::now();
        ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                                          TransferFunction::makeSmoke(),
                                          renderSettings));
        renderer.flushAndWait();
        const QImage image =
            renderer.readbackTextureViewToImage(volumeRenderer.outputSRV());
        const double renderMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - renderStart)
                .count();
        ASSERT_EQ(image.size(), QSize(outputSize, outputSize));
        const auto memoryEstimate = simulation.estimateMemory();

        double alphaMass = 0.0;
        double alphaMomentY = 0.0;
        std::array<double, 3> alphaThirdMass{};
        double alphaWeightedLumaMass = 0.0;
        double alphaWeightedLumaMomentY = 0.0;
        std::array<double, 3> alphaWeightedLumaThirdMass{};
        std::array<std::size_t, 3> visiblePixelsByThird{};
        for (int y = 0; y < outputSize; ++y) {
          const std::size_t third = std::min<std::size_t>(
              2, 3 * static_cast<std::size_t>(y) / outputSize);
          for (int x = 0; x < outputSize; ++x) {
            const QColor pixel = image.pixelColor(x, y);
            const float alpha = pixel.alphaF();
            const double alphaWeightedLuma = alpha * (
                0.2126 * pixel.redF() + 0.7152 * pixel.greenF() +
                0.0722 * pixel.blueF());
            alphaMass += alpha;
            // Match densityCentroidY01's world-up convention.
            alphaMomentY += alpha * (static_cast<double>(outputSize) - y - 0.5);
            alphaThirdMass[third] += alpha;
            alphaWeightedLumaMass += alphaWeightedLuma;
            alphaWeightedLumaMomentY += alphaWeightedLuma *
                (static_cast<double>(outputSize) - y - 0.5);
            alphaWeightedLumaThirdMass[third] += alphaWeightedLuma;
            if (alpha > 0.04f)
              ++visiblePixelsByThird[third];
          }
        }
        ASSERT_GT(alphaMass, 0.0);
        ASSERT_GT(alphaWeightedLumaMass, 0.0);
        GTEST_LOG_(INFO)
            << "Pyro solver resolution=" << gridSize << '^' << 3
            << " pressureIterations=" << profile.simulation.pressureIterations
            << " condition=" << condition.name
            << " sourceVelocityScale=" << condition.sourceVelocityScale
            << " buoyancyScale=" << condition.buoyancyScale
            << " smokeTurbulence=" << profile.simulation.smokeTurbulence
            << " smokeTurbulenceScale="
            << profile.simulation.smokeTurbulenceScale
            << " substepsPerFrame=" << substepsPerFrame
            << " totalSteps=" << simulationSteps
            << " buoyancyCellsPerSecondSquared=" << profile.simulation.buoyancy
            << " physicalDensityMass=" << physicalDensityMass
            << " densityCentroidY01="
            << densityMomentY / physicalDensityMass / resolution.height
            << " upperThirdDensityShare="
            << upperThirdDensityMass / physicalDensityMass
            << " densityThirdShares(bottom/middle/top)="
            << lowerThirdDensityMass / physicalDensityMass << '/'
            << middleThirdDensityMass / physicalDensityMass << '/'
            << upperThirdDensityMass / physicalDensityMass
            << " densityYBounds01=" << densityMinY << '/' << densityMaxY
            << " boundaryDensityShares(xMin/xMax/yMin/yMax/zMin/zMax)="
            << boundaryDensityMass[0] / physicalDensityMass << '/'
            << boundaryDensityMass[1] / physicalDensityMass << '/'
            << boundaryDensityMass[2] / physicalDensityMass << '/'
            << boundaryDensityMass[3] / physicalDensityMass << '/'
            << boundaryDensityMass[4] / physicalDensityMass << '/'
            << boundaryDensityMass[5] / physicalDensityMass
            << " outputAlphaCentroidY01="
            << alphaMomentY / alphaMass / outputSize
            << " outputAlphaThirdShares(top/middle/bottom)="
            << alphaThirdMass[0] / alphaMass << '/'
            << alphaThirdMass[1] / alphaMass << '/'
            << alphaThirdMass[2] / alphaMass
            << " alphaWeightedLumaCentroidY01="
            << alphaWeightedLumaMomentY / alphaWeightedLumaMass / outputSize
            << " alphaWeightedLumaThirdShares(top/middle/bottom)="
            << alphaWeightedLumaThirdMass[0] / alphaWeightedLumaMass << '/'
            << alphaWeightedLumaThirdMass[1] / alphaWeightedLumaMass << '/'
            << alphaWeightedLumaThirdMass[2] / alphaWeightedLumaMass
            << " visiblePixels(top/middle/bottom)=" << visiblePixelsByThird[0]
            << '/' << visiblePixelsByThird[1] << '/' << visiblePixelsByThird[2]
            << " fieldMemoryMiB="
            << memoryEstimate.totalBytes / (1024.0 * 1024.0)
            << " projectedVelocityDivergenceRms="
            << std::sqrt(divergenceSquaredSum /
                         static_cast<double>(divergenceSamples))
            << " raySteps=" << renderSettings.raySteps
            << " simulationMs/renderFlushReadbackMs=" << simulationMilliseconds
            << '/' << renderMilliseconds;
        if (!outputDirectory.isEmpty()) {
          EXPECT_TRUE(image.save(
              QDir(outputDirectory)
                  .filePath(
                      QStringLiteral("pyro-smoke-solver-%1-grid-%2-cubed.png")
                          .arg(QString::fromLatin1(condition.name))
                          .arg(gridSize)),
              "PNG"));
        }
      }
    }
}

TEST(ClonerGpuRenderContractTest, PyroAdvectionPrototypePreservesTranslatedDensity)
{
    using namespace ArtifactCore;
    constexpr int width = 48, height = 32, depth = 32, steps = 24;
    constexpr float velocity = 0.55f, sigma = 2.3f;
    const PyroResolution resolution{width, height, depth};
    const std::size_t cellCount = static_cast<std::size_t>(width) * height * depth;
    const auto indexOf = [](int x, int y, int z) {
        return static_cast<std::size_t>(x + width * (y + height * z));
    };
    const auto analytic = [](float x, float y, float z, float centerX) {
        const float dx = x - centerX, dy = y - 16.0f, dz = z - 16.0f;
        return std::exp(-(dx * dx + dy * dy + dz * dz) / (2.0f * sigma * sigma));
    };
    PyroFieldSet sampler;
    sampler.resize(resolution);
    Array<float> firstOrder, corrected, nextFirstOrder, forward, nextCorrected;
    firstOrder.resize(cellCount);
    corrected.resize(cellCount);
    nextFirstOrder.resize(cellCount);
    forward.resize(cellCount);
    nextCorrected.resize(cellCount);
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                firstOrder[indexOf(x, y, z)] = analytic(
                    static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 10.0f);
    corrected = firstOrder;
    for (int step = 0; step < steps; ++step) {
        const PyroConstScalarFieldView firstView{
            std::span<const float>(firstOrder.data(), firstOrder.size()), resolution};
        const PyroConstScalarFieldView correctedView{
            std::span<const float>(corrected.data(), corrected.size()), resolution};
        for (int z = 0; z < depth; ++z)
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    const PyroVec3 departure{static_cast<float>(x) - velocity,
                                             static_cast<float>(y), static_cast<float>(z)};
                    const auto cell = indexOf(x, y, z);
                    nextFirstOrder[cell] = sampler.sampleScalar(firstView, departure, PyroBoundaryMode::Open);
                    forward[cell] = sampler.sampleScalar(correctedView, departure, PyroBoundaryMode::Open);
                }
        const PyroConstScalarFieldView forwardView{
            std::span<const float>(forward.data(), forward.size()), resolution};
        for (int z = 0; z < depth; ++z)
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    const auto cell = indexOf(x, y, z);
                    const float departureX = static_cast<float>(x) - velocity;
                    const int x0 = static_cast<int>(std::floor(departureX));
                    if (x0 < 0 || x0 + 1 >= width) {
                        nextCorrected[cell] = forward[cell];
                        continue;
                    }
                    const float reverse = sampler.sampleScalar(forwardView,
                        {static_cast<float>(x) + velocity, static_cast<float>(y), static_cast<float>(z)},
                        PyroBoundaryMode::Open);
                    float low = corrected[indexOf(x0, y, z)];
                    float high = low;
                    for (int dz = 0; dz <= 1; ++dz)
                        for (int dy = 0; dy <= 1; ++dy)
                            for (int dx = 0; dx <= 1; ++dx) {
                                const float value = corrected[indexOf(x0 + dx,
                                    std::min(y + dy, height - 1), std::min(z + dz, depth - 1))];
                                low = std::min(low, value);
                                high = std::max(high, value);
                            }
                    nextCorrected[cell] = std::clamp(
                        forward[cell] + 0.5f * (corrected[cell] - reverse), low, high);
                }
        firstOrder.swap(nextFirstOrder);
        corrected.swap(nextCorrected);
    }
    double firstError = 0.0, correctedError = 0.0;
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const auto cell = indexOf(x, y, z);
                const float exact = analytic(static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(z), 10.0f + velocity * steps);
                const double a = static_cast<double>(firstOrder[cell]) - exact;
                const double b = static_cast<double>(corrected[cell]) - exact;
                firstError += a * a;
                correctedError += b * b;
                ASSERT_GE(corrected[cell], 0.0f);
                ASSERT_LE(corrected[cell], 1.0f);
            }
    const double firstRms = std::sqrt(firstError / cellCount);
    const double correctedRms = std::sqrt(correctedError / cellCount);
    GTEST_LOG_(INFO) << "Pyro translation advection RMS first/corrected="
                     << firstRms << '/' << correctedRms
                     << " peak first/corrected=" << *std::max_element(firstOrder.begin(), firstOrder.end())
                     << '/' << *std::max_element(corrected.begin(), corrected.end());
    EXPECT_LT(correctedRms, firstRms * 0.8);
}

TEST(ClonerGpuRenderContractTest, PyroMaterialCoordinatesFollowKnownRotation)
{
    using namespace ArtifactCore;
    constexpr int size = 24, depth = 8, steps = 8;
    constexpr float angleStep = 0.025f;
    constexpr float center = (size - 1) * 0.5f;
    const PyroResolution resolution{size, size, depth};
    PyroFieldSet sampler;
    sampler.resize(resolution);
    std::vector<PyroVec3> current(sampler.cellCount()), next(current.size());
    const auto indexOf = [](int x, int y, int z) {
        return static_cast<std::size_t>(x) + size *
            (static_cast<std::size_t>(y) + size * static_cast<std::size_t>(z));
    };
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                current[indexOf(x, y, z)] = {static_cast<float>(x),
                    static_cast<float>(y), static_cast<float>(z)};
    const float cosine = std::cos(angleStep), sine = std::sin(angleStep);
    for (int step = 0; step < steps; ++step) {
        const PyroConstVectorFieldView input{
            std::span<const PyroVec3>(current.data(), current.size()), resolution};
        for (int z = 0; z < depth; ++z)
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x) {
                    const float dx = x - center, dy = y - center;
                    const PyroVec3 departure{center + cosine * dx + sine * dy,
                                             center - sine * dx + cosine * dy,
                                             static_cast<float>(z)};
                    next[indexOf(x, y, z)] = sampler.sampleVector(
                        input, departure, PyroBoundaryMode::Closed);
                }
        current.swap(next);
    }
    const float c = std::cos(angleStep * steps), sn = std::sin(angleStep * steps);
    double advectedError = 0.0, fixedError = 0.0;
    std::size_t count = 0;
    for (int z = 1; z < depth - 1; ++z)
        for (int y = 6; y < size - 6; ++y)
            for (int x = 6; x < size - 6; ++x) {
                const float dx = x - center, dy = y - center;
                const float expectedX = center + c * dx + sn * dy;
                const float expectedY = center - sn * dx + c * dy;
                const auto& actual = current[indexOf(x, y, z)];
                advectedError += std::pow(actual.x - expectedX, 2) +
                                  std::pow(actual.y - expectedY, 2);
                fixedError += std::pow(x - expectedX, 2) + std::pow(y - expectedY, 2);
                ++count;
            }
    const double advectedRms = std::sqrt(advectedError / count);
    const double fixedRms = std::sqrt(fixedError / count);
    GTEST_LOG_(INFO) << "Pyro material coordinates rotation RMS advected/fixed="
                     << advectedRms << '/' << fixedRms;
    EXPECT_LT(advectedRms, 0.001);
    EXPECT_GT(fixedRms, 0.1);
}

TEST(ClonerGpuRenderContractTest, PyroVelocityAdvectionPrototypePreservesShear)
{
    // Divergence-free shear v=(constant, f(x), 0), passively translated by
    // a prescribed constant velocity. Pressure and self-advection are absent.
    constexpr int width = 96;
    constexpr int steps = 30;
    constexpr float shift = 0.45f;
    const auto initial = [](float x) {
        const float q = x - 30.0f;
        return std::exp(-q * q / 50.0f) * std::sin(q * 0.7f);
    };
    std::array<float, width> first{}, corrected{}, forward{}, nextFirst{}, nextCorrected{};
    for (int x = 0; x < width; ++x) first[x] = corrected[x] = initial(static_cast<float>(x));
    const auto sample = [](const auto& values, float x) {
        if (x < 0.0f || x > width - 1.0f) return 0.0f;
        const int low = static_cast<int>(std::floor(x));
        const int high = std::min(low + 1, width - 1);
        return std::lerp(values[low], values[high], x - low);
    };
    for (int step = 0; step < steps; ++step) {
        for (int x = 0; x < width; ++x) {
            nextFirst[x] = sample(first, x - shift);
            forward[x] = sample(corrected, x - shift);
        }
        for (int x = 0; x < width; ++x) {
            const float departure = x - shift;
            if (departure < 0.0f || x + shift > width - 1.0f) {
                nextCorrected[x] = forward[x];
                continue;
            }
            const int low = static_cast<int>(std::floor(departure));
            const int high = std::min(low + 1, width - 1);
            nextCorrected[x] = std::clamp(
                forward[x] + 0.5f * (corrected[x] - sample(forward, x + shift)),
                std::min(corrected[low], corrected[high]),
                std::max(corrected[low], corrected[high]));
        }
        first.swap(nextFirst);
        corrected.swap(nextCorrected);
    }
    double firstError = 0.0, correctedError = 0.0;
    double firstCurlError = 0.0, correctedCurlError = 0.0;
    for (int x = 1; x < width - 1; ++x) {
        const float exact = initial(x - shift * steps);
        const float exactCurl = 0.5f * (initial(x + 1 - shift * steps) -
                                       initial(x - 1 - shift * steps));
        firstError += std::pow(first[x] - exact, 2);
        correctedError += std::pow(corrected[x] - exact, 2);
        firstCurlError += std::pow(0.5f * (first[x + 1] - first[x - 1]) - exactCurl, 2);
        correctedCurlError += std::pow(0.5f * (corrected[x + 1] - corrected[x - 1]) - exactCurl, 2);
        ASSERT_TRUE(std::isfinite(corrected[x]));
    }
    GTEST_LOG_(INFO) << "Pyro shear translated velocity RMS first/corrected="
        << std::sqrt(firstError / (width - 2)) << '/'
        << std::sqrt(correctedError / (width - 2)) << " curl RMS="
        << std::sqrt(firstCurlError / (width - 2)) << '/'
        << std::sqrt(correctedCurlError / (width - 2));
    EXPECT_LT(correctedError, firstError);
    EXPECT_LT(correctedCurlError, firstCurlError);
}

// Diagnostic remains disabled until finite-domain boundary effects are isolated.
TEST(ClonerGpuRenderContractTest, DISABLED_PyroSolverAdvectsDivergenceFreeShear)
{
    using namespace ArtifactCore;
    constexpr int steps = 30;
    const bool stationaryShear = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SHEAR_STATIONARY") != 0;
    const float shift = stationaryShear ? 0.0f : 0.45f;
    const bool largeDomain = qEnvironmentVariableIntValue("ARTIFACT_PYRO_SHEAR_LARGE") != 0;
    const bool closedDomain = qEnvironmentVariableIntValue("ARTIFACT_PYRO_SHEAR_CLOSED") != 0;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {9.6f, largeDomain ? 6.4f : 1.6f, largeDomain ? 3.2f : 0.8f},
        .voxelSize = 0.1f, .fixedTimeStep = 1.0f / 60.0f,
        .boundaryMode = closedDomain ? PyroBoundaryMode::Closed : PyroBoundaryMode::Open};
    PyroSimulation simulation(domain);
    auto settings = PyroSimulationSettings{};
    settings.dissipation = 0.0f;
    settings.coolingRate = 0.0f;
    settings.buoyancy = 0.0f;
    settings.vorticity = 0.0f;
    bool pressureValid = false;
    const int pressureIterations = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SHEAR_PRESSURE_ITERATIONS", &pressureValid);
    settings.pressureIterations = static_cast<float>(
        pressureValid ? std::clamp(pressureIterations, 1, 64) : 8);
    simulation.setSettings(settings);
    simulation.setCacheInterval(0);
    const auto resolution = simulation.fields().resolution();
    const auto shear = [](float x) {
        const float q = x - 30.0f;
        return std::exp(-q * q / 50.0f) * std::sin(q * 0.7f);
    };
    auto& velocity = simulation.fields().velocityStorage();
    const auto indexOf = [&](int x, int y, int z) {
        return static_cast<std::size_t>(x) + resolution.width *
            (static_cast<std::size_t>(y) + resolution.height * static_cast<std::size_t>(z));
    };
    for (int z = 0; z < resolution.depth; ++z)
        for (int y = 0; y < resolution.height; ++y)
            for (int x = 0; x < resolution.width; ++x)
                velocity[indexOf(x, y, z)] = {shift * 60.0f, shear(static_cast<float>(x)), 0.0f};
    for (int step = 0; step < steps; ++step) simulation.step(1.0 / 60.0);
    double error = 0.0, curlError = 0.0;
    std::size_t count = 0;
    const int y = resolution.height / 2;
    const int z = resolution.depth / 2;
    for (int x = 15; x < 75; ++x) {
        const float actual = velocity[indexOf(x, y, z)].y;
        const float actualCurl = 0.5f * (velocity[indexOf(x + 1, y, z)].y -
                                         velocity[indexOf(x - 1, y, z)].y);
        const float exact = shear(x - shift * steps);
        const float exactCurl = 0.5f * (shear(x + 1 - shift * steps) -
                                       shear(x - 1 - shift * steps));
        ASSERT_TRUE(std::isfinite(actual));
        error += std::pow(actual - exact, 2);
        curlError += std::pow(actualCurl - exactCurl, 2);
        ++count;
    }
    const double rms = std::sqrt(error / count);
    const double curlRms = std::sqrt(curlError / count);
    GTEST_LOG_(INFO) << "Pyro solver shear velocity/curl RMS=" << rms << '/' << curlRms;
    EXPECT_LT(rms, 0.4);
    EXPECT_LT(curlRms, 0.4);
}

TEST(ClonerGpuRenderContractTest, PyroHighOrderDensityAdvectionInSolver)
{
    using namespace ArtifactCore;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {4.8f, 3.2f, 3.2f},
        .voxelSize = 0.1f,
        .fixedTimeStep = 1.0f / 60.0f,
        .boundaryMode = PyroBoundaryMode::Open};
    auto baselineSettings = PyroSimulationSettings{};
    baselineSettings.dissipation = 0.0f;
    baselineSettings.coolingRate = 0.0f;
    baselineSettings.buoyancy = 0.0f;
    baselineSettings.vorticity = 0.0f;
    baselineSettings.pressureIterations = 1.0f;
    auto correctedSettings = baselineSettings;
    correctedSettings.highOrderDensityAdvection = true;
    EXPECT_NE(hashSettings(baselineSettings), hashSettings(correctedSettings));
    const auto simulate = [&](const PyroSimulationSettings& settings) {
        PyroSimulation simulation(domain);
        simulation.setSettings(settings);
        simulation.setCacheInterval(0);
        auto& density = simulation.fields().densityStorage();
        auto& velocity = simulation.fields().velocityStorage();
        const auto resolution = simulation.fields().resolution();
        for (int z = 0; z < resolution.depth; ++z)
            for (int y = 0; y < resolution.height; ++y)
                for (int x = 0; x < resolution.width; ++x) {
                    const auto cell = static_cast<std::size_t>(x) +
                        static_cast<std::size_t>(resolution.width) *
                        (static_cast<std::size_t>(y) + static_cast<std::size_t>(resolution.height) * z);
                    const float dx = x - 10.0f, dy = y - 16.0f, dz = z - 16.0f;
                    density[cell] = std::exp(-(dx * dx + dy * dy + dz * dz) / (2.0f * 2.3f * 2.3f));
                    velocity[cell] = {33.0f, 0.0f, 0.0f};
                }
        for (int step = 0; step < 24; ++step) simulation.step(1.0 / 60.0);
        return simulation.fields().densityStorage();
    };
    const auto first = simulate(baselineSettings);
    const auto corrected = simulate(correctedSettings);
    const auto repeated = simulate(correctedSettings);
    ASSERT_EQ(first.size(), corrected.size());
    EXPECT_EQ(corrected, repeated);
    double firstError = 0.0, correctedError = 0.0;
    const auto resolution = domain.resolution();
    for (int z = 0; z < resolution.depth; ++z)
        for (int y = 0; y < resolution.height; ++y)
            for (int x = 0; x < resolution.width; ++x) {
                const auto cell = static_cast<std::size_t>(x) +
                    static_cast<std::size_t>(resolution.width) *
                    (static_cast<std::size_t>(y) + static_cast<std::size_t>(resolution.height) * z);
                const float dx = x - (10.0f + 0.55f * 24.0f), dy = y - 16.0f, dz = z - 16.0f;
                const float exact = std::exp(-(dx * dx + dy * dy + dz * dz) / (2.0f * 2.3f * 2.3f));
                const double a = static_cast<double>(first[cell]) - exact;
                const double b = static_cast<double>(corrected[cell]) - exact;
                firstError += a * a;
                correctedError += b * b;
                ASSERT_TRUE(std::isfinite(corrected[cell]));
                ASSERT_GE(corrected[cell], 0.0f);
                ASSERT_LE(corrected[cell], 1.0f);
            }
    const double firstRms = std::sqrt(firstError / first.size());
    const double correctedRms = std::sqrt(correctedError / corrected.size());
    GTEST_LOG_(INFO) << "Pyro solver advection RMS first/corrected=" << firstRms << '/' << correctedRms;
    EXPECT_LT(correctedRms, firstRms);
}

TEST(ClonerGpuRenderContractTest, PyroAnisotropicSourceUsesEachAxisExtent)
{
    using namespace ArtifactCore;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {1.0f, 1.0f, 1.0f},
        .voxelSize = 0.025f,
        .fixedTimeStep = 1.0f / 60.0f};
    PyroSimulation simulation(domain);
    PyroSimulationSettings settings;
    settings.dissipation = 0.0f;
    settings.buoyancy = 0.0f;
    simulation.setSettings(settings);
    simulation.setCacheInterval(0);
    PyroSourceState source;
    source.position = {0.5f, 0.5f, 0.5f};
    source.extent = {0.4f, 0.1f, 0.2f};
    source.density = 1.0f;
    simulation.setSources(std::span<const PyroSourceState>(&source, 1));
    simulation.step(domain.fixedTimeStep);
    const auto density = simulation.fields().densityView();
    ASSERT_GT(density.at(23, 20, 20), 0.0f);
    EXPECT_LT(density.at(20, 23, 20), density.at(23, 20, 20) * 0.3f);
}

TEST(ClonerGpuRenderContractTest, PyroFieldMemoryEstimateIncludesReaction)
{
    using namespace ArtifactCore;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {1.0f, 2.0f, 1.0f},
        .voxelSize = 1.0f / 32.0f};
    PyroSimulation simulation(domain);
    const auto fields = simulation.fields().estimateMemory();
    EXPECT_GT(fields.reactionBytes, 0u);
    EXPECT_EQ(estimatePyroMemoryBytes(domain.resolution()), fields.totalBytes);
}

TEST(ClonerGpuRenderContractTest, PyroZeroCacheIntervalDisablesCheckpoints)
{
    using namespace ArtifactCore;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {0.4f, 0.4f, 0.4f},
        .voxelSize = 0.05f,
        .fixedTimeStep = 1.0f / 60.0f,
        .boundaryMode = PyroBoundaryMode::Open};
    PyroSimulation simulation(domain);
    simulation.setCacheInterval(0);
    ASSERT_EQ(simulation.cacheInterval(), 0u);
    simulation.step(1.0 / 60.0);
    ASSERT_EQ(simulation.frameIndex(), 1u);
    simulation.fields().densityStorage()[0] = 7.0f;
    simulation.seek(1, 1.0 / 60.0);
    EXPECT_FLOAT_EQ(simulation.fields().densityStorage()[0], 7.0f)
        << "zero interval should not create a checkpoint";

    simulation.setCacheInterval(1);
    simulation.step(1.0 / 60.0);
    ASSERT_EQ(simulation.frameIndex(), 2u);
    const float saved = simulation.fields().densityStorage()[0];
    simulation.fields().densityStorage()[0] = saved + 7.0f;
    simulation.seek(2, 2.0 / 60.0);
    EXPECT_FLOAT_EQ(simulation.fields().densityStorage()[0], saved)
        << "positive intervals must still restore checkpoints";
}

TEST(ClonerGpuRenderContractTest, PyroAdvectionModesUseSeparateDiskCheckpoints)
{
    using namespace ArtifactCore;
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {0.4f, 0.4f, 0.4f},
        .voxelSize = 0.05f,
        .fixedTimeStep = 1.0f / 60.0f,
        .boundaryMode = PyroBoundaryMode::Open};
    PyroSimulation simulation(domain);
    simulation.setCacheDirectory(std::filesystem::path(directory.path().toStdWString()));
    simulation.setCacheInterval(1);
    simulation.step(1.0 / 60.0);
    ASSERT_EQ(simulation.frameIndex(), 1u);
    auto settings = simulation.settings();
    settings.highOrderDensityAdvection = true;
    simulation.setSettings(settings);
    simulation.fields().densityStorage()[0] = 7.0f;
    simulation.seek(1, 1.0 / 60.0);
    EXPECT_FLOAT_EQ(simulation.fields().densityStorage()[0], 7.0f)
        << "a checkpoint made by first-order advection must not be restored";
    simulation.step(1.0 / 60.0);
    ASSERT_EQ(simulation.frameIndex(), 2u);
    const float savedHighOrder = simulation.fields().densityStorage()[0];
    simulation.reset();
    ASSERT_TRUE(simulation.loadCheckpointFromDisk(2).loaded);
    EXPECT_FLOAT_EQ(simulation.fields().densityStorage()[0], savedHighOrder);
    settings.highOrderDensityAdvection = false;
    simulation.setSettings(settings);
    EXPECT_FALSE(simulation.loadCheckpointFromDisk(2).loaded);
    EXPECT_TRUE(simulation.loadCheckpointFromDisk(1).loaded);
}

TEST(ClonerGpuRenderContractTest, PyroLegacyCheckpointIsNotReused)
{
    using namespace ArtifactCore;
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {0.4f, 0.4f, 0.4f},
        .voxelSize = 0.05f,
        .fixedTimeStep = 1.0f / 60.0f};
    PyroSimulation simulation(domain);
    simulation.setCacheDirectory(std::filesystem::path(directory.path().toStdWString()));
    simulation.setCacheInterval(1);
    simulation.step(1.0 / 60.0);
    const QString currentPath = QDir(directory.path()).filePath(QStringLiteral("1.v3.pyroc"));
    const QString legacyPath = QDir(directory.path()).filePath(QStringLiteral("1.pyroc"));
    ASSERT_TRUE(QFile::copy(currentPath, legacyPath));
    QFile legacy(legacyPath);
    ASSERT_TRUE(legacy.open(QIODevice::ReadWrite));
    ASSERT_TRUE(legacy.seek(4));
    const std::uint32_t oldVersion = 2;
    ASSERT_EQ(legacy.write(reinterpret_cast<const char*>(&oldVersion), sizeof(oldVersion)),
              static_cast<qint64>(sizeof(oldVersion)));
    legacy.close();
    ASSERT_TRUE(QFile::remove(currentPath));
    simulation.reset();
    EXPECT_FALSE(simulation.loadCheckpointFromDisk(1).loaded);
    ASSERT_TRUE(QFile::copy(legacyPath, currentPath));
    EXPECT_FALSE(simulation.loadCheckpointFromDisk(1).loaded);
    EXPECT_TRUE(QFile::exists(legacyPath));
}

TEST(ClonerGpuRenderContractTest, PyroGpuSmokeSequenceRemainsInFrame)
{
    using namespace ArtifactCore;
    using namespace ArtifactCore::RayTrace;
    bool gridValid = false;
    const int requestedGrid = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_GRID", &gridValid);
    const int grid = gridValid ? std::clamp(requestedGrid, 24, 96) : 24;
    bool widthValid = false;
    const int requestedWidth = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_WIDTH", &widthValid);
    const int width = widthValid ? std::clamp(requestedWidth, 96, 512) : 192;
    const int height = width * 2;
    const PyroDomain domain{
        .min = {0.0f, 0.0f, 0.0f},
        .max = {1.2f, 2.4f, 1.2f},
        .voxelSize = 1.2f / static_cast<float>(grid),
        .fixedTimeStep = 1.0f / 60.0f,
        .boundaryMode = PyroBoundaryMode::Open};
    const auto resolution = domain.resolution();
    const VolumeResolution volumeResolution{
        resolution.width, resolution.height, resolution.depth};
    auto profile = makePyroEffectProfile(PyroEffectKind::Smoke, resolution);
    bool dissipationValid = false;
    const float requestedDissipation = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_DISSIPATION").toFloat(&dissipationValid);
    if (dissipationValid && std::isfinite(requestedDissipation)) {
        profile.simulation.dissipation = std::clamp(requestedDissipation, 0.0f, 1.0f);
    }
    bool vorticityValid = false;
    const float requestedVorticity = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_VORTICITY").toFloat(&vorticityValid);
    if (vorticityValid && std::isfinite(requestedVorticity)) {
        profile.simulation.vorticity = std::clamp(requestedVorticity, 0.0f, 100.0f);
    }
    bool curlForceValid = false;
    const float requestedCurlForce = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_CURL_FORCE").toFloat(&curlForceValid);
    const float curlForce = curlForceValid && std::isfinite(requestedCurlForce)
        ? std::clamp(requestedCurlForce, 0.0f, 100.0f) : 0.0f;
    const bool localizedCurl = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_LOCAL_CURL") != 0;
    const bool curlNoise = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_CURL_NOISE") != 0;
    const bool smoothCurlNoiseEnvelope = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_CURL_NOISE_SMOOTH") != 0;
    const bool exponentialDecay = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_EXP_DECAY") != 0;
    const float experimentalDissipation = profile.simulation.dissipation;
    if (exponentialDecay) profile.simulation.dissipation = 0.0f;
    bool pressureValid = false;
    const int pressureIterations = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_PRESSURE_ITERATIONS", &pressureValid);
    if (pressureValid)
        profile.simulation.pressureIterations = static_cast<float>(
            std::clamp(pressureIterations, 1, 64));
    profile.simulation.highOrderDensityAdvection =
        qEnvironmentVariableIntValue("ARTIFACT_PYRO_HIGH_ORDER_DENSITY") != 0;
    profile.source.position = {0.6f, 0.35f, 0.6f};
    profile.source.extent = {0.16f, 0.16f, 0.16f};
    if (qEnvironmentVariableIntValue("ARTIFACT_PYRO_SEQUENCE_WIDE_SOURCE") != 0) {
        // Preserve the analytic ellipsoid volume while changing its aspect ratio.
        profile.source.extent = {0.24f, 0.0711111f, 0.24f};
    }
    PyroSimulation simulation(domain);
    simulation.setSettings(profile.simulation);
    simulation.setCacheInterval(0);
    ASSERT_EQ(simulation.cacheInterval(), 0u);

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(renderer.device(), renderer.immediateContext()));
    GpuVolumeRenderSettings settings{
        .outputWidth = width,
        .outputHeight = height,
        .raySteps = 96,
        .densityScale = 1.0f,
        .temperatureScale = 1.0f,
        .opacity = 1.0f};
    bool densityScaleValid = false;
    const float requestedDensityScale = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_DENSITY_SCALE").toFloat(&densityScaleValid);
    if (densityScaleValid && std::isfinite(requestedDensityScale)) {
        settings.densityScale = std::clamp(requestedDensityScale, 0.01f, 10.0f);
    }
    settings.detailStrength = 0.7f;
    settings.detailSeed = 2026u;
    bool detailStrengthValid = false;
    const float requestedDetailStrength = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_DETAIL_STRENGTH").toFloat(&detailStrengthValid);
    if (detailStrengthValid && std::isfinite(requestedDetailStrength))
        settings.detailStrength = std::clamp(requestedDetailStrength, 0.0f, 1.0f);
    bool detailScaleValid = false;
    const float requestedDetailScale = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_DETAIL_SCALE").toFloat(&detailScaleValid);
    if (detailScaleValid && std::isfinite(requestedDetailScale))
        settings.detailScale = std::clamp(requestedDetailScale, 1.0f, 64.0f);
    settings.smokeLightingStrength = 1.0f;
    bool rayStepsValid = false;
    const int requestedRaySteps = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_RAY_STEPS", &rayStepsValid);
    if (rayStepsValid) settings.raySteps = std::clamp(requestedRaySteps, 1, 1024);
    bool lightingValid = false;
    const float requestedLighting = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_LIGHTING").toFloat(&lightingValid);
    if (lightingValid && std::isfinite(requestedLighting))
        settings.smokeLightingStrength = std::clamp(requestedLighting, 0.0f, 1.0f);
    // Fixed framing over the source and its expected plume; no per-frame tracking.
    settings.viewCenterX = 0.5f;
    settings.viewCenterY = 0.225f;
    settings.viewScaleX = 0.45f;
    settings.viewScaleY = 0.45f;
    if (qEnvironmentVariableIntValue("ARTIFACT_PYRO_SEQUENCE_FULL_VIEW") != 0) {
        settings.viewCenterY = 0.5f;
        settings.viewScaleX = 1.0f;
        settings.viewScaleY = 1.0f;
    }
    const int longSequence = qEnvironmentVariableIntValue("ARTIFACT_PYRO_SEQUENCE_LONG");
    const bool materialRender = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_MATERIAL_RENDER") != 0;
    const std::array<int, 5> captureFrames = materialRender
        ? std::array<int, 5>{60, 61, 62, 90, 91}
        : longSequence > 1 ? std::array<int, 5>{24, 192, 288, 336, 384}
        : longSequence == 1 ? std::array<int, 5>{24, 96, 144, 168, 192}
                            : std::array<int, 5>{24, 48, 72, 73, 96};
    std::array<QImage, captureFrames.size()> captures;
    const QString outputDirectory = qEnvironmentVariable("ARTIFACT_PYRO_SEQUENCE_OUTPUT_DIR");
    if (!outputDirectory.isEmpty()) ASSERT_TRUE(QDir().mkpath(outputDirectory));
    std::size_t captureIndex = 0;
    bool thresholdValid = false;
    const int requestedAlphaThreshold = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_BORDER_ALPHA_THRESHOLD", &thresholdValid);
    const int borderAlphaThreshold = thresholdValid
        ? std::clamp(requestedAlphaThreshold, 0, 254) : 8;
    const bool checkAllFrames = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_CHECK_ALL_FRAMES") != 0;
    const bool variedSources = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_VARIED_SOURCES") != 0;
    const bool spreadSources = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_SPREAD_SOURCES") != 0;
    bool sourceFramesValid = false;
    const int sourceFrames = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_SOURCE_FRAMES", &sourceFramesValid);
    const bool autoFrame = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_AUTO_FRAME") != 0;
    const bool momentFrame = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_MOMENT_FRAME") != 0;
    const bool materialCoordinatesEnabled = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_MATERIAL_COORDINATES") != 0;
    bool resetIntervalValid = false;
    const int requestedResetInterval = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_MATERIAL_RESET_INTERVAL", &resetIntervalValid);
    const int materialResetInterval = resetIntervalValid
        ? std::clamp(requestedResetInterval, 0, 10000) : 0;
    const bool upsampleMaterial = materialRender && materialCoordinatesEnabled &&
        qEnvironmentVariableIntValue("ARTIFACT_PYRO_SEQUENCE_MATERIAL_UPSAMPLE") != 0;
    const bool dualMaterial = materialCoordinatesEnabled && materialResetInterval > 1 &&
        qEnvironmentVariableIntValue("ARTIFACT_PYRO_SEQUENCE_MATERIAL_DUAL") != 0;
    std::vector<PyroVec3> materialCoordinates, materialScratch, materialSecond, materialSecondScratch;
    std::vector<float> previousMaterialSignal, materialDensity, materialTemperature;
    const auto materialIndex = [&](int x, int y, int z) {
        return static_cast<std::size_t>(x) + resolution.width *
            (static_cast<std::size_t>(y) + resolution.height * static_cast<std::size_t>(z));
    };
    if (materialCoordinatesEnabled) {
        materialCoordinates.resize(simulation.fields().cellCount());
        materialScratch.resize(materialCoordinates.size());
        if (materialRender) {
            const std::size_t factor = upsampleMaterial ? 8u : 1u;
            materialDensity.resize(materialCoordinates.size() * factor);
            if (upsampleMaterial) materialTemperature.resize(materialDensity.size(), 0.0f);
        }
        if (materialResetInterval > 1) previousMaterialSignal.resize(materialCoordinates.size());
        if (dualMaterial) {
            materialSecond.resize(materialCoordinates.size());
            materialSecondScratch.resize(materialCoordinates.size());
        }
        for (int z = 0; z < resolution.depth; ++z)
            for (int y = 0; y < resolution.height; ++y)
                for (int x = 0; x < resolution.width; ++x)
                    materialCoordinates[materialIndex(x, y, z)] = {
                        static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    }
    if (dualMaterial) materialSecond = materialCoordinates;
    const bool noiseMaterial = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_MATERIAL_NOISE") != 0;
    bool detailZScaleValid = false;
    const float requestedDetailZScale = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_MATERIAL_DETAIL_Z_SCALE").toFloat(&detailZScaleValid);
    const float materialDetailZScale = detailZScaleValid && std::isfinite(requestedDetailZScale)
        ? std::clamp(requestedDetailZScale, 0.1f, 2.0f) : 1.0f;
    const auto materialSignal = [noiseMaterial, materialDetailZScale](const PyroVec3& p) {
        if (!noiseMaterial) return std::sin(p.x * 0.9f + std::sin(p.y * 0.7f) + p.z * 0.5f);
        const auto noise = [](float x, float y, float z) {
            const int ix = static_cast<int>(std::floor(x));
            const int iy = static_cast<int>(std::floor(y));
            const int iz = static_cast<int>(std::floor(z));
            const auto smooth = [](float t) { return t * t * (3.0f - 2.0f * t); };
            const float tx = smooth(x - ix), ty = smooth(y - iy), tz = smooth(z - iz);
            const auto hash = [](int a, int b, int c) {
                std::uint32_t h = static_cast<std::uint32_t>(a) * 1597334677u ^
                    static_cast<std::uint32_t>(b) * 3812015801u ^
                    static_cast<std::uint32_t>(c) * 2798796415u;
                h ^= h >> 16u; h *= 2246822519u; h ^= h >> 13u;
                return static_cast<float>(h >> 8u) * (2.0f / 16777215.0f) - 1.0f;
            };
            const float z0 = std::lerp(std::lerp(hash(ix,iy,iz), hash(ix+1,iy,iz), tx),
                                       std::lerp(hash(ix,iy+1,iz), hash(ix+1,iy+1,iz), tx), ty);
            const float z1 = std::lerp(std::lerp(hash(ix,iy,iz+1), hash(ix+1,iy,iz+1), tx),
                                       std::lerp(hash(ix,iy+1,iz+1), hash(ix+1,iy+1,iz+1), tx), ty);
            return std::lerp(z0, z1, tz);
        };
        return 0.65f * noise(p.x * 0.25f, p.y * 0.25f, p.z * 0.25f * materialDetailZScale) +
               0.35f * noise(p.x * 0.65f + 17.0f, p.y * 0.65f + 11.0f,
                             p.z * 0.65f * materialDetailZScale + 5.0f);
    };
    bool materialDetailStrengthValid = false;
    const float requestedMaterialDetailStrength = qEnvironmentVariable(
        "ARTIFACT_PYRO_SEQUENCE_MATERIAL_DETAIL_STRENGTH").toFloat(
            &materialDetailStrengthValid);
    const float materialDetailStrength = materialDetailStrengthValid &&
        std::isfinite(requestedMaterialDetailStrength)
        ? std::clamp(requestedMaterialDetailStrength, 0.0f, 3.0f) : 0.8f;
    const bool normalizeMaterialMass = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_MATERIAL_NORMALIZE_MASS") != 0;
    std::array<std::vector<float>, 3> curlPotential;
    if (curlNoise && curlForce > 0.0f) {
        const auto noise3 = [](float x, float y, float z) {
            const int ix = static_cast<int>(std::floor(x));
            const int iy = static_cast<int>(std::floor(y));
            const int iz = static_cast<int>(std::floor(z));
            const auto fade = [](float t) { return t * t * (3.0f - 2.0f * t); };
            const float tx = fade(x - ix), ty = fade(y - iy), tz = fade(z - iz);
            const auto hash = [](int a, int b, int c) {
                std::uint32_t h = static_cast<std::uint32_t>(a) * 1597334677u ^
                    static_cast<std::uint32_t>(b) * 3812015801u ^
                    static_cast<std::uint32_t>(c) * 2798796415u;
                h ^= h >> 16u; h *= 2246822519u; h ^= h >> 13u;
                return static_cast<float>(h >> 8u) * (2.0f / 16777215.0f) - 1.0f;
            };
            const auto samplePlane = [&](int zIndex) {
                const float a = std::lerp(hash(ix, iy, zIndex), hash(ix + 1, iy, zIndex), tx);
                const float b = std::lerp(hash(ix, iy + 1, zIndex), hash(ix + 1, iy + 1, zIndex), tx);
                return std::lerp(a, b, ty);
            };
            const float z0 = samplePlane(iz);
            const float z1 = samplePlane(iz + 1);
            return std::lerp(z0, z1, tz);
        };
        const std::array<PyroVec3, 3> offsets{{
            {17.0f, 3.0f, 11.0f}, {5.0f, 23.0f, 7.0f}, {13.0f, 9.0f, 29.0f}}};
        for (auto& component : curlPotential)
            component.resize(simulation.fields().cellCount());
        for (int z = 0; z < resolution.depth; ++z)
            for (int y = 0; y < resolution.height; ++y)
                for (int x = 0; x < resolution.width; ++x) {
                    const auto index = materialIndex(x, y, z);
                    for (std::size_t component = 0; component < curlPotential.size(); ++component) {
                        const auto& offset = offsets[component];
                        const float px = static_cast<float>(x) + offset.x;
                        const float py = static_cast<float>(y) + offset.y;
                        const float pz = static_cast<float>(z) + offset.z;
                        curlPotential[component][index] =
                            0.65f * noise3(px * 0.18f, py * 0.18f, pz * 0.18f) +
                            0.35f * noise3(px * 0.36f, py * 0.36f, pz * 0.36f);
                    }
                }
    }
    const bool followDetail = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_FOLLOW_DETAIL") != 0;
    const bool freezeDetailTime = qEnvironmentVariableIntValue(
        "ARTIFACT_PYRO_SEQUENCE_FREEZE_DETAIL_TIME") != 0;
    float maximumViewDelta = 0.0f;
    float targetCenterX = settings.viewCenterX;
    float targetCenterY = settings.viewCenterY;
    float targetScale = settings.viewScaleX;
    bool previousTargetValid = false;
    float maximumTargetDelta = 0.0f;
    std::size_t targetChangeFrames = 0;
    for (int frame = 1; frame <= captureFrames.back(); ++frame) {
        std::array<PyroSourceState, 3> sources{
            profile.source, profile.source, profile.source};
        if (variedSources) {
            const float time = static_cast<float>(frame) * domain.fixedTimeStep;
            for (std::size_t index = 0; index < sources.size(); ++index) {
                const float phase = static_cast<float>(index) * 2.0943951f;
                const float radius = spreadSources ? 0.2f : 0.09f;
                sources[index].position.x += radius * std::sin(time * 2.3f + phase);
                sources[index].position.z += radius * std::cos(time * 1.7f + phase);
                sources[index].extent = {0.09f, 0.09f, 0.09f};
                sources[index].density /= static_cast<float>(sources.size());
                sources[index].temperature /= static_cast<float>(sources.size());
                sources[index].velocity.x = 8.0f * std::cos(time * 2.3f + phase);
                sources[index].velocity.z = -8.0f * std::sin(time * 1.7f + phase);
                sources[index].velocity.y /= static_cast<float>(sources.size());
            }
        }
        const bool sourceActive = !sourceFramesValid || frame <= std::max(0, sourceFrames);
        simulation.setSources(sourceActive
            ? std::span<const PyroSourceState>(sources.data(), variedSources ? sources.size() : 1)
            : std::span<const PyroSourceState>{});
        if (exponentialDecay) {
            // Mirror existing velocity damping while testing multiplicative
            // density decay before source injection, entirely in this fixture.
            const float dt = static_cast<float>(domain.fixedTimeStep);
            const float densityRetention = std::exp(-experimentalDissipation * dt);
            const float velocityRetention = std::max(0.0f, 1.0f - experimentalDissipation * dt);
            for (auto& value : simulation.fields().densityStorage()) value *= densityRetention;
            for (auto& value : simulation.fields().velocityStorage()) {
                value.x *= velocityRetention; value.y *= velocityRetention; value.z *= velocityRetention;
            }
        }
        if (curlForce > 0.0f) {
            // Curl of a smooth XY streamfunction, modulated along Z. Test-only
            // acceleration in grid-cells/s^2; no per-step allocation.
            auto& velocities = simulation.fields().velocityStorage();
            const float k = 6.28318530718f / (resolution.width * 0.6f);
            const float phase = static_cast<float>(frame * domain.fixedTimeStep) * 0.45f;
            const float increment = curlForce * static_cast<float>(domain.fixedTimeStep);
            for (int z = 0; z < resolution.depth; ++z)
                for (int y = 0; y < resolution.height; ++y)
                    for (int x = 0; x < resolution.width; ++x) {
                        const float px = k * (x + 0.5f) + phase;
                        const float py = k * (y + 0.5f) - phase * 0.7f;
                        const float modulation = std::cos(k * (z + 0.5f) + phase * 0.3f);
                        auto& v = velocities[static_cast<std::size_t>(x) +
                            resolution.width * (static_cast<std::size_t>(y) +
                            resolution.height * static_cast<std::size_t>(z))];
                        if (localizedCurl) {
                            const float radius = resolution.width * 0.16f;
                            const float cy = resolution.height * 0.23f;
                            const float dz = (z + 0.5f - resolution.depth * 0.5f) / radius;
                            for (float side : {-1.0f, 1.0f}) {
                                const float cx = resolution.width * (0.5f + side * 0.10f);
                                const float dx = (x + 0.5f - cx) / radius;
                                const float dy = (y + 0.5f - cy) / radius;
                                const float envelope = std::exp(-0.5f * (dx * dx + dy * dy + dz * dz));
                                v.x -= side * increment * dy * envelope;
                                v.y += side * increment * dx * envelope;
                            }
                        } else if (curlNoise) {
                            const auto densityIndex = static_cast<std::size_t>(x) +
                                resolution.width * (static_cast<std::size_t>(y) +
                                resolution.height * static_cast<std::size_t>(z));
                            const float density = simulation.fields().densityStorage()[densityIndex];
                            if (density < 0.002f) continue;
                            const float gateT = smoothCurlNoiseEnvelope
                                ? std::clamp((density - 0.002f) / 0.028f, 0.0f, 1.0f) : 1.0f;
                            const float densityGate = smoothCurlNoiseEnvelope
                                ? gateT * gateT * (3.0f - 2.0f * gateT) : 1.0f;
                            const auto sample = [&](std::size_t component, int sx, int sy, int sz) {
                                sx = std::clamp(sx, 0, resolution.width - 1);
                                sy = std::clamp(sy, 0, resolution.height - 1);
                                sz = std::clamp(sz, 0, resolution.depth - 1);
                                const auto sampleIndex = static_cast<std::size_t>(sx) +
                                    resolution.width * (static_cast<std::size_t>(sy) +
                                    resolution.height * static_cast<std::size_t>(sz));
                                return curlPotential[component][sampleIndex];
                            };
                            const float curlX = 0.5f * (
                                sample(2, x, y + 1, z) - sample(2, x, y - 1, z) -
                                sample(1, x, y, z + 1) + sample(1, x, y, z - 1));
                            const float curlY = 0.5f * (
                                sample(0, x, y, z + 1) - sample(0, x, y, z - 1) -
                                sample(2, x + 1, y, z) + sample(2, x - 1, y, z));
                            const float curlZ = 0.5f * (
                                sample(1, x + 1, y, z) - sample(1, x - 1, y, z) -
                                sample(0, x, y + 1, z) + sample(0, x, y - 1, z));
                            const float noiseScale = increment * 4.0f;
                            v.x += noiseScale * densityGate * curlX;
                            v.y += noiseScale * densityGate * curlY;
                            v.z += noiseScale * densityGate * curlZ;
                        } else {
                            v.x += increment * std::sin(px) * std::cos(py) * modulation;
                            v.y -= increment * std::cos(px) * std::sin(py) * modulation;
                        }
                    }
        }
        simulation.step(domain.fixedTimeStep);
        if (materialCoordinatesEnabled) {
            if (materialResetInterval > 0 && frame > 1 &&
                (frame - 1) % materialResetInterval == 0) {
                const auto& density = simulation.fields().densityStorage();
                double squaredJump = 0.0;
                std::size_t measured = 0;
                for (int z = 0; z < resolution.depth; ++z)
                    for (int y = 0; y < resolution.height; ++y)
                        for (int x = 0; x < resolution.width; ++x) {
                            const auto index = materialIndex(x, y, z);
                            auto& position = materialCoordinates[index];
                            if (density[index] >= 0.002f) {
                                squaredJump += std::pow(position.x - x, 2) +
                                    std::pow(position.y - y, 2) + std::pow(position.z - z, 2);
                                ++measured;
                            }
                            position = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                        }
                GTEST_LOG_(INFO) << "Pyro material reset frame=" << frame
                    << " cells=" << measured << " rms-jump-cells="
                    << std::sqrt(squaredJump / std::max<std::size_t>(1, measured));
            }
            if (dualMaterial && frame > 1 &&
                (frame - 1) % materialResetInterval == materialResetInterval / 2) {
                for (int z = 0; z < resolution.depth; ++z)
                    for (int y = 0; y < resolution.height; ++y)
                        for (int x = 0; x < resolution.width; ++x)
                            materialSecond[materialIndex(x, y, z)] = {
                                static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
            }
            const auto& velocity = simulation.fields().velocityStorage();
            const PyroConstVectorFieldView input{
                std::span<const PyroVec3>(materialCoordinates.data(), materialCoordinates.size()), resolution};
            const PyroConstVectorFieldView secondInput{
                std::span<const PyroVec3>(materialSecond.data(), materialSecond.size()), resolution};
            const float dt = static_cast<float>(domain.fixedTimeStep);
            for (int z = 0; z < resolution.depth; ++z)
                for (int y = 0; y < resolution.height; ++y)
                    for (int x = 0; x < resolution.width; ++x) {
                        const auto index = materialIndex(x, y, z);
                        const auto& v = velocity[index];
                        const PyroVec3 departure{x - dt * v.x, y - dt * v.y, z - dt * v.z};
                        const bool outside = departure.x < 0 || departure.y < 0 || departure.z < 0 ||
                            departure.x > resolution.width - 1 || departure.y > resolution.height - 1 ||
                            departure.z > resolution.depth - 1;
                        materialScratch[index] = outside
                            ? PyroVec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}
                            : simulation.fields().sampleVector(input, departure, PyroBoundaryMode::Closed);
                        if (dualMaterial) materialSecondScratch[index] = outside
                            ? PyroVec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}
                            : simulation.fields().sampleVector(secondInput, departure, PyroBoundaryMode::Closed);
                    }
            materialCoordinates.swap(materialScratch);
            if (dualMaterial) materialSecond.swap(materialSecondScratch);
            if (materialResetInterval > 1) {
                const float phase = static_cast<float>((frame - 1) % materialResetInterval) /
                    static_cast<float>(materialResetInterval);
                const float primaryWeight = dualMaterial ?
                    std::pow(std::sin(3.14159265359f * phase), 2.0f) : 1.0f;
                const auto& signal = materialSignal;
                const auto& density = simulation.fields().densityStorage();
                double squaredDelta = 0.0;
                std::size_t measured = 0;
                for (std::size_t i = 0; i < materialCoordinates.size(); ++i) {
                    const float value = primaryWeight * signal(materialCoordinates[i]) +
                        (dualMaterial ? (1.0f - primaryWeight) * signal(materialSecond[i]) : 0.0f);
                    if (frame > 1 && density[i] >= 0.002f) {
                        squaredDelta += std::pow(value - previousMaterialSignal[i], 2);
                        ++measured;
                    }
                    previousMaterialSignal[i] = value;
                }
                const int remainder = (frame - 1) % materialResetInterval;
                if (frame > 1 && (remainder <= 1 ||
                    remainder == materialResetInterval / 2 ||
                    remainder == materialResetInterval / 2 + 1)) {
                    GTEST_LOG_(INFO) << "Pyro material signal delta frame=" << frame
                        << " dual=" << dualMaterial << " rms="
                        << std::sqrt(squaredDelta / std::max<std::size_t>(1, measured));
                }
            }
        }
        if (followDetail) {
            const auto& density = simulation.fields().densityStorage();
            const auto& velocity = simulation.fields().velocityStorage();
            double weight = 0.0, vx = 0.0, vy = 0.0, vz = 0.0;
            for (std::size_t i = 0; i < density.size(); ++i) {
                const double d = std::max(0.0f, density[i]);
                weight += d; vx += d * velocity[i].x;
                vy += d * velocity[i].y; vz += d * velocity[i].z;
            }
            if (weight > 1.0e-12) {
                const double dt = domain.fixedTimeStep;
                settings.detailOffsetX -= static_cast<float>(dt * vx / weight / resolution.width);
                settings.detailOffsetY -= static_cast<float>(dt * vy / weight / resolution.height);
                settings.detailOffsetZ -= static_cast<float>(dt * vz / weight / resolution.depth);
            }
        }
        if (autoFrame) {
            const auto density = simulation.fields().densityView();
            int minX = resolution.width, minY = resolution.height;
            int maxX = -1, maxY = -1;
            double mass = 0.0, mx = 0.0, my = 0.0, mxx = 0.0, myy = 0.0;
            for (int z = 0; z < resolution.depth; ++z) {
                for (int y = 0; y < resolution.height; ++y) {
                    for (int x = 0; x < resolution.width; ++x) {
                        const float value = density.at(x, y, z);
                        if (momentFrame && value > 0.0f) {
                            const double nx = (x + 0.5) / resolution.width;
                            const double ny = (y + 0.5) / resolution.height;
                            mass += value; mx += value * nx; my += value * ny;
                            mxx += value * nx * nx; myy += value * ny * ny;
                        }
                        if (value < 0.002f) continue;
                        minX = std::min(minX, x); maxX = std::max(maxX, x);
                        minY = std::min(minY, y); maxY = std::max(maxY, y);
                    }
                }
            }
            if (maxX >= minX && maxY >= minY) {
                float centerX = (minX + maxX + 1.0f) / (2.0f * resolution.width);
                float centerY = (minY + maxY + 1.0f) / (2.0f * resolution.height);
                const float spanX = (maxX - minX + 1.0f) / resolution.width;
                const float spanY = (maxY - minY + 1.0f) / resolution.height;
                float scale = std::clamp(1.5f * std::max(spanX, spanY), 0.3f, 1.0f);
                if (momentFrame && mass > 0.0) {
                    centerX = static_cast<float>(mx / mass);
                    centerY = static_cast<float>(my / mass);
                    const double sx = std::sqrt(std::max(0.0, mxx / mass - centerX * centerX));
                    const double sy = std::sqrt(std::max(0.0, myy / mass - centerY * centerY));
                    scale = std::clamp(static_cast<float>(6.0 * std::max(sx, sy)), 0.3f, 1.0f);
                }
                if (previousTargetValid) {
                    const float change = std::max({std::abs(centerX - targetCenterX),
                        std::abs(centerY - targetCenterY), std::abs(scale - targetScale)});
                    maximumTargetDelta = std::max(maximumTargetDelta, change);
                    targetChangeFrames += change > 0.0f ? 1u : 0u;
                }
                previousTargetValid = true;
                targetCenterX = centerX;
                targetCenterY = centerY;
                targetScale = scale;
                const float response = momentFrame ? 1.0f : 0.08f;
                const float dx = (centerX - settings.viewCenterX) * response;
                const float dy = (centerY - settings.viewCenterY) * response;
                const float ds = (scale - settings.viewScaleX) * response;
                maximumViewDelta = std::max({maximumViewDelta, std::abs(dx),
                                             std::abs(dy), std::abs(ds)});
                settings.viewCenterX += dx; settings.viewCenterY += dy;
                settings.viewScaleX += ds; settings.viewScaleY = settings.viewScaleX;
            }
        }
        const bool captureFrame = frame == captureFrames[captureIndex];
        if (!captureFrame && (!checkAllFrames || frame < captureFrames.front())) continue;
        const auto& densityValues = simulation.fields().densityStorage();
        ASSERT_TRUE(std::all_of(densityValues.begin(), densityValues.end(), [](float value) {
            return std::isfinite(value) && value >= 0.0f;
        })) << "invalid smoke density at frame " << frame;
        if (materialRender && materialCoordinatesEnabled) {
            const auto& density = simulation.fields().densityStorage();
            double baseMaterialMass = 0.0;
            double detailedMaterialMass = 0.0;
            const float phase = materialResetInterval > 1
                ? static_cast<float>((frame - 1) % materialResetInterval) / materialResetInterval : 0.0f;
            const float weight = dualMaterial ? std::pow(std::sin(3.14159265359f * phase), 2.0f) : 1.0f;
            const auto& signal = materialSignal;
            if (upsampleMaterial) {
                const PyroConstScalarFieldView scalar{
                    std::span<const float>(density.data(), density.size()), resolution};
                const PyroConstVectorFieldView first{
                    std::span<const PyroVec3>(materialCoordinates.data(), materialCoordinates.size()), resolution};
                const PyroConstVectorFieldView second{
                    std::span<const PyroVec3>(materialSecond.data(), materialSecond.size()), resolution};
                const int w = resolution.width * 2, h = resolution.height * 2, d = resolution.depth * 2;
                for (int z = 0; z < d; ++z)
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x) {
                            const PyroVec3 position{(x + 0.5f) * 0.5f - 0.5f,
                                (y + 0.5f) * 0.5f - 0.5f, (z + 0.5f) * 0.5f - 0.5f};
                            const float base = simulation.fields().sampleScalar(scalar, position, PyroBoundaryMode::Closed);
                            const auto p1 = simulation.fields().sampleVector(first, position, PyroBoundaryMode::Closed);
                            const float p2 = dualMaterial ? signal(simulation.fields().sampleVector(
                                second, position, PyroBoundaryMode::Closed)) : 0.0f;
                            const float detail = weight * signal(p1) + (1.0f - weight) * p2;
                            const auto i = static_cast<std::size_t>(x) + w *
                                (static_cast<std::size_t>(y) + h * static_cast<std::size_t>(z));
                            materialDensity[i] = base * std::max(0.0f, 1.0f + materialDetailStrength * detail);
                            baseMaterialMass += base;
                            detailedMaterialMass += materialDensity[i];
                        }
            } else {
                for (std::size_t i = 0; i < density.size(); ++i) {
                    const float detail = weight * signal(materialCoordinates[i]) +
                        (dualMaterial ? (1.0f - weight) * signal(materialSecond[i]) : 0.0f);
                    materialDensity[i] = density[i] * std::max(0.0f, 1.0f + materialDetailStrength * detail);
                    baseMaterialMass += density[i];
                    detailedMaterialMass += materialDensity[i];
                }
            }
            if (normalizeMaterialMass && detailedMaterialMass > 1.0e-12) {
                const float massScale = static_cast<float>(baseMaterialMass / detailedMaterialMass);
                for (auto& value : materialDensity) value *= massScale;
                GTEST_LOG_(INFO) << "Pyro material density mass normalization frame=" << frame
                    << " scale=" << massScale;
            }
            settings.detailStrength = 0.0f;
        }
        const VolumeResolution renderResolution = upsampleMaterial
            ? VolumeResolution{resolution.width * 2, resolution.height * 2, resolution.depth * 2}
            : volumeResolution;
        VolumeFieldSet fields{
            .density = {materialRender && materialCoordinatesEnabled ? materialDensity.data() :
                        simulation.fields().densityStorage().data(), renderResolution},
            .temperature = {upsampleMaterial ? materialTemperature.data() :
                            simulation.fields().temperatureStorage().data(), renderResolution},
            .reaction = upsampleMaterial ? VolumeScalarField{} : VolumeScalarField{
                simulation.fields().reactionStorage().data(), volumeResolution}};
        if (captureFrame && upsampleMaterial && !outputDirectory.isEmpty()) {
            const int w = renderResolution.width, h = renderResolution.height;
            const int z = renderResolution.depth / 2;
            QImage baseSlice(w, h, QImage::Format_Grayscale8);
            QImage detailSlice(w, h, QImage::Format_Grayscale8);
            const auto& baseStorage = simulation.fields().densityStorage();
            const PyroConstScalarFieldView scalar{
                std::span<const float>(baseStorage.data(), baseStorage.size()), resolution};
            double deltaSquared = 0.0, baselineSquared = 0.0;
            for (int y = 0; y < h; ++y) {
                auto* baseRow = baseSlice.scanLine(y);
                auto* detailRow = detailSlice.scanLine(y);
                for (int x = 0; x < w; ++x) {
                    const PyroVec3 pos{(x + 0.5f) * 0.5f - 0.5f,
                        (y + 0.5f) * 0.5f - 0.5f, (z + 0.5f) * 0.5f - 0.5f};
                    const float base = simulation.fields().sampleScalar(scalar, pos, PyroBoundaryMode::Closed);
                    const auto index = static_cast<std::size_t>(x) + w *
                        (static_cast<std::size_t>(y) + h * static_cast<std::size_t>(z));
                    const float detail = materialDensity[index];
                    deltaSquared += (detail - base) * (detail - base);
                    baselineSquared += base * base;
                    baseRow[x] = static_cast<std::uint8_t>(std::lround(std::clamp(base, 0.0f, 1.0f) * 255));
                    detailRow[x] = static_cast<std::uint8_t>(std::lround(std::clamp(detail, 0.0f, 1.0f) * 255));
                }
            }
            GTEST_LOG_(INFO) << "Pyro material center slice relative RMS frame=" << frame
                << " value=" << std::sqrt(deltaSquared / std::max(1.0e-20, baselineSquared));
            EXPECT_TRUE(baseSlice.save(QDir(outputDirectory).filePath(
                QStringLiteral("slice-%1-base.png").arg(frame, 3, 10, QLatin1Char('0'))), "PNG"));
            EXPECT_TRUE(detailSlice.save(QDir(outputDirectory).filePath(
                QStringLiteral("slice-%1-detail.png").arg(frame, 3, 10, QLatin1Char('0'))), "PNG"));
        }
        settings.detailTimeSeconds = freezeDetailTime ? 0.0f : static_cast<float>(frame) / 60.0f;
        ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
            ArtifactCore::RayTrace::TransferFunction::makeSmoke(), settings));
        renderer.flushAndWait();
        const QImage image = renderer.readbackTextureViewToImage(volumeRenderer.outputSRV());
        if (captureFrame) captures[captureIndex] = image;
        ASSERT_FALSE(image.isNull());
        std::size_t covered = 0;
        std::size_t border = 0;
        double weightedY = 0.0;
        double alphaSum = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const int alpha = image.pixelColor(x, y).alpha();
                if (alpha > 8) ++covered;
                if (alpha > borderAlphaThreshold &&
                    (x == 0 || y == 0 || x == width - 1 || y == height - 1))
                    ++border;
                alphaSum += alpha;
                weightedY += static_cast<double>(alpha) * y;
            }
        }
        EXPECT_GT(covered, 0u) << frame;
        EXPECT_EQ(border, 0u) << "smoke touches the frame at " << frame
            << " above alpha threshold " << borderAlphaThreshold;
        if (alphaSum > 0.0) {
            EXPECT_GT(weightedY / alphaSum, 0.15 * height) << frame;
            EXPECT_LT(weightedY / alphaSum, 0.85 * height) << frame;
        }
        if (captureFrame) {
            const auto& velocities = simulation.fields().velocityStorage();
            const auto& density = simulation.fields().densityStorage();
            const auto velocityAt = [&](int x, int y, int z) -> const PyroVec3& {
                return velocities[static_cast<std::size_t>(x) + resolution.width *
                    (static_cast<std::size_t>(y) + resolution.height * static_cast<std::size_t>(z))];
            };
            double curlSquared = 0.0;
            double divergenceSquared = 0.0;
            std::size_t cells = 0;
            for (int z = 1; z < resolution.depth - 1; ++z)
                for (int y = 1; y < resolution.height - 1; ++y)
                    for (int x = 1; x < resolution.width - 1; ++x) {
                        const auto& xp = velocityAt(x + 1, y, z);
                        const auto& xm = velocityAt(x - 1, y, z);
                        const auto& yp = velocityAt(x, y + 1, z);
                        const auto& ym = velocityAt(x, y - 1, z);
                        const auto& zp = velocityAt(x, y, z + 1);
                        const auto& zm = velocityAt(x, y, z - 1);
                        const double wx = 0.5 * ((yp.z - ym.z) - (zp.y - zm.y));
                        const double wy = 0.5 * ((zp.x - zm.x) - (xp.z - xm.z));
                        const double wz = 0.5 * ((xp.y - xm.y) - (yp.x - ym.x));
                        const double divergence = 0.5 * ((xp.x - xm.x) +
                            (yp.y - ym.y) + (zp.z - zm.z));
                        curlSquared += wx * wx + wy * wy + wz * wz;
                        divergenceSquared += divergence * divergence;
                        ++cells;
                    }
            GTEST_LOG_(INFO) << "Pyro velocity curl rms frame=" << frame
                << " value=" << std::sqrt(curlSquared / std::max<std::size_t>(1, cells))
                << " post-projection divergence-rms="
                << std::sqrt(divergenceSquared / std::max<std::size_t>(1, cells));
            double densityMass = 0.0, densityMomentY = 0.0;
            double lowerThirdMass = 0.0, upperThirdMass = 0.0;
            float peakDensity = 0.0f;
            for (const float d : density) peakDensity = std::max(peakDensity, d);
            int minActiveY = resolution.height, maxActiveY = -1;
            for (int z = 0; z < resolution.depth; ++z)
                for (int y = 0; y < resolution.height; ++y)
                    for (int x = 0; x < resolution.width; ++x) {
                        const auto cell = static_cast<std::size_t>(x) + resolution.width *
                            (static_cast<std::size_t>(y) + resolution.height * static_cast<std::size_t>(z));
                        const double d = std::max(0.0f, density[cell]);
                        densityMass += d;
                        densityMomentY += d * (y + 0.5);
                        if (y < resolution.height / 3) lowerThirdMass += d;
                        if (y >= (2 * resolution.height) / 3) upperThirdMass += d;
                        if (density[cell] >= 0.05f * peakDensity) {
                            minActiveY = std::min(minActiveY, y);
                            maxActiveY = std::max(maxActiveY, y);
                        }
                    }
            GTEST_LOG_(INFO) << "Pyro density vertical distribution frame=" << frame
                << " mass=" << densityMass
                << " centroidY01=" << (densityMass > 0.0 ? densityMomentY / densityMass / resolution.height : 0.0)
                << " lowerThirdFraction=" << (densityMass > 0.0 ? lowerThirdMass / densityMass : 0.0)
                << " upperThirdFraction=" << (densityMass > 0.0 ? upperThirdMass / densityMass : 0.0)
                << " peak5pctYBounds=" << minActiveY << ':' << maxActiveY;
            if (qEnvironmentVariableIntValue("ARTIFACT_PYRO_SEQUENCE_WAVELET_DIAGNOSTIC") != 0) {
                // Test-only dyadic block variance estimate. This is not an
                // orthogonal wavelet transform; it measures density-weighted
                // velocity detail removed by successive 2x2x2 averaging.
                std::vector<PyroVec3> coarseVelocity = velocities;
                std::vector<float> coarseDensity = simulation.fields().densityStorage();
                int coarseWidth = resolution.width;
                int coarseHeight = resolution.height;
                int coarseDepth = resolution.depth;
                int band = 0;
                while (coarseWidth >= 2 && coarseHeight >= 2 && coarseDepth >= 2) {
                    const int nextWidth = coarseWidth / 2;
                    const int nextHeight = coarseHeight / 2;
                    const int nextDepth = coarseDepth / 2;
                    std::vector<PyroVec3> nextVelocity(static_cast<std::size_t>(nextWidth) * nextHeight * nextDepth);
                    std::vector<float> nextDensity(nextVelocity.size(), 0.0f);
                    double weightedVariance = 0.0;
                    double densityWeight = 0.0;
                    for (int z = 0; z < nextDepth; ++z)
                        for (int y = 0; y < nextHeight; ++y)
                            for (int x = 0; x < nextWidth; ++x) {
                                double densitySum = 0.0;
                                double vx = 0.0, vy = 0.0, vz = 0.0;
                                for (int dz = 0; dz < 2; ++dz)
                                    for (int dy = 0; dy < 2; ++dy)
                                        for (int dx = 0; dx < 2; ++dx) {
                                            const auto source = static_cast<std::size_t>(2*x+dx) + coarseWidth *
                                                (static_cast<std::size_t>(2*y+dy) + coarseHeight * static_cast<std::size_t>(2*z+dz));
                                            const double d = std::max(0.0f, coarseDensity[source]);
                                            const auto& v = coarseVelocity[source];
                                            densitySum += d;
                                            vx += d * v.x; vy += d * v.y; vz += d * v.z;
                                        }
                                const auto target = static_cast<std::size_t>(x) + nextWidth *
                                    (static_cast<std::size_t>(y) + nextHeight * static_cast<std::size_t>(z));
                                if (densitySum > 1.0e-12) {
                                    vx /= densitySum; vy /= densitySum; vz /= densitySum;
                                    for (int dz = 0; dz < 2; ++dz)
                                        for (int dy = 0; dy < 2; ++dy)
                                            for (int dx = 0; dx < 2; ++dx) {
                                                const auto source = static_cast<std::size_t>(2*x+dx) + coarseWidth *
                                                    (static_cast<std::size_t>(2*y+dy) + coarseHeight * static_cast<std::size_t>(2*z+dz));
                                                const double d = std::max(0.0f, coarseDensity[source]);
                                                const auto& v = coarseVelocity[source];
                                                const double ax = v.x-vx, ay = v.y-vy, az = v.z-vz;
                                                weightedVariance += 0.5 * d * (ax*ax + ay*ay + az*az);
                                            }
                                    densityWeight += densitySum;
                                    nextVelocity[target] = {static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)};
                                }
                                nextDensity[target] = static_cast<float>(densitySum / 8.0);
                            }
                    GTEST_LOG_(INFO) << "Pyro dyadic velocity detail frame=" << frame
                        << " band=" << band << " density-weighted-variance="
                        << (densityWeight > 0.0 ? weightedVariance / densityWeight : 0.0)
                        << " density-weight=" << densityWeight;
                    coarseVelocity.swap(nextVelocity);
                    coarseDensity.swap(nextDensity);
                    coarseWidth = nextWidth; coarseHeight = nextHeight; coarseDepth = nextDepth;
                    ++band;
                }
            }
        }
        if (captureFrame && materialCoordinatesEnabled) {
            const auto& density = simulation.fields().densityStorage();
            double minDeterminant = 1.0e30, maxDeterminant = -1.0e30;
            std::size_t measured = 0;
            for (int z = 1; z < resolution.depth - 1; ++z)
                for (int y = 1; y < resolution.height - 1; ++y)
                    for (int x = 1; x < resolution.width - 1; ++x) {
                        if (density[materialIndex(x, y, z)] < 0.002f) continue;
                        const auto& xp = materialCoordinates[materialIndex(x + 1, y, z)];
                        const auto& xm = materialCoordinates[materialIndex(x - 1, y, z)];
                        const auto& yp = materialCoordinates[materialIndex(x, y + 1, z)];
                        const auto& ym = materialCoordinates[materialIndex(x, y - 1, z)];
                        const auto& zp = materialCoordinates[materialIndex(x, y, z + 1)];
                        const auto& zm = materialCoordinates[materialIndex(x, y, z - 1)];
                        const double ax = (xp.x-xm.x)*0.5, ay = (xp.y-xm.y)*0.5, az = (xp.z-xm.z)*0.5;
                        const double bx = (yp.x-ym.x)*0.5, by = (yp.y-ym.y)*0.5, bz = (yp.z-ym.z)*0.5;
                        const double cx = (zp.x-zm.x)*0.5, cy = (zp.y-zm.y)*0.5, cz = (zp.z-zm.z)*0.5;
                        const double det = ax*(by*cz-bz*cy) - bx*(ay*cz-az*cy) + cx*(ay*bz-az*by);
                        ASSERT_TRUE(std::isfinite(det));
                        minDeterminant = std::min(minDeterminant, det);
                        maxDeterminant = std::max(maxDeterminant, det);
                        ++measured;
                    }
            GTEST_LOG_(INFO) << "Pyro material Jacobian frame=" << frame
                << " cells=" << measured << " min/max=" << minDeterminant << '/' << maxDeterminant;
            EXPECT_GT(measured, 0u);
        }
        if (captureFrame) GTEST_LOG_(INFO) << "Pyro smoke sequence frame=" << frame
                         << " grid=" << resolution.width << 'x' << resolution.height
                         << 'x' << resolution.depth << " output=" << width << 'x' << height
                         << " visible=" << covered
                         << " alphaCentroidY=" << (alphaSum > 0.0 ? weightedY / alphaSum : 0.0);
        if (captureFrame && !outputDirectory.isEmpty()) {
            EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(
                QStringLiteral("smoke-%1.png").arg(frame, 3, 10, QLatin1Char('0'))), "PNG"));
            // The readback PNG contains straight sRGB and alpha. Make a
            // separate black-background preview using linear-light source-over.
            QImage blackPreview(image.size(), QImage::Format_RGB32);
            const auto compositeChannel = [](int channel, int alpha) {
                const double encoded = channel / 255.0;
                const double linear = encoded <= 0.04045
                    ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
                const double composite = linear * (alpha / 255.0);
                const double result = composite <= 0.0031308
                    ? composite * 12.92 : 1.055 * std::pow(composite, 1.0 / 2.4) - 0.055;
                return std::clamp(static_cast<int>(std::lround(result * 255.0)), 0, 255);
            };
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const QColor pixel = image.pixelColor(x, y);
                    blackPreview.setPixel(x, y, qRgb(
                        compositeChannel(pixel.red(), pixel.alpha()),
                        compositeChannel(pixel.green(), pixel.alpha()),
                        compositeChannel(pixel.blue(), pixel.alpha())));
                }
            }
            EXPECT_TRUE(blackPreview.save(QDir(outputDirectory).filePath(
                QStringLiteral("smoke-%1-black.png").arg(frame, 3, 10, QLatin1Char('0'))), "PNG"));
        }
        if (autoFrame && frame == captureFrames.back()) {
            // Compare the same field with history-smoothed and direct framing.
            const auto filteredSettings = settings;
            settings.viewCenterX = targetCenterX;
            settings.viewCenterY = targetCenterY;
            settings.viewScaleX = targetScale;
            settings.viewScaleY = targetScale;
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(), fields,
                ArtifactCore::RayTrace::TransferFunction::makeSmoke(), settings));
            renderer.flushAndWait();
            const QImage direct = renderer.readbackTextureViewToImage(volumeRenderer.outputSRV());
            ASSERT_FALSE(direct.isNull());
            GTEST_LOG_(INFO) << "Pyro auto-frame direct vs filtered parameters="
                << std::abs(targetCenterX - filteredSettings.viewCenterX) << '/'
                << std::abs(targetCenterY - filteredSettings.viewCenterY) << '/'
                << std::abs(targetScale - filteredSettings.viewScaleX)
                << " identical=" << (direct == image);
            if (!outputDirectory.isEmpty()) {
                EXPECT_TRUE(direct.save(QDir(outputDirectory).filePath(
                    QStringLiteral("smoke-direct-final.png")), "PNG"));
            }
            settings = filteredSettings;
        }
        if (captureFrame) ++captureIndex;
    }
    if (autoFrame) {
        GTEST_LOG_(INFO) << "Pyro auto-frame maximum per-frame parameter change="
                         << maximumViewDelta << " direct maximum="
                         << maximumTargetDelta << " direct change frames="
                         << targetChangeFrames;
    }
    for (std::size_t index = 1; index < captures.size(); ++index) {
        EXPECT_NE(captures[index - 1], captures[index]);
    }
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererAsyncDeferredRecordingPreservesOverlayOrder)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    const ArtifactCore::FloatColor clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const ArtifactCore::FloatColor invisibleColor(1.0f, 1.0f, 1.0f, 1.0f);
    const ArtifactCore::FloatColor baseColor(0.95f, 0.1f, 0.1f, 1.0f);
    const ArtifactCore::FloatColor overlayColor(0.1f, 0.2f, 0.95f, 1.0f);

    const auto drawBase = [&] {
        for (int index = 0; index < 16; ++index) {
            renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                   -40.0f, 2.0f, 2.0f, invisibleColor);
        }
        renderer.drawSolidRect(16.0f, 16.0f, 64.0f, 64.0f, baseColor);
    };
    const auto drawOverlay = [&] {
        renderer.drawSolidRect(48.0f, 48.0f, 64.0f, 64.0f, overlayColor);
    };

    renderer.setClearColor(clearColor);
    renderer.clear();
    drawBase();
    renderer.flush();
    drawOverlay();
    renderer.flushAndWait();
    const auto immediateImage = renderer.readbackToImageF32();

    const auto workerSubmitCountBeforeAsync =
        renderer.deferredWorkerSubmitCount();
    renderer.clear();
    drawBase();
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the supported base packet batch should be handed to the worker";
    drawOverlay();
    renderer.flushAndWait();
    const auto asyncImage = renderer.readbackToImageF32();
    ASSERT_EQ(asyncImage.width(), immediateImage.width());
    ASSERT_EQ(asyncImage.height(), immediateImage.height());
    float maxChannelError = 0.0f;
    for (int y = 0; y < asyncImage.height(); ++y) {
        for (int x = 0; x < asyncImage.width(); ++x) {
            const auto expected = immediateImage.getPixel(x, y);
            const auto actual = asyncImage.getPixel(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(actual.r() - expected.r()),
                std::abs(actual.g() - expected.g()),
                std::abs(actual.b() - expected.b()),
                std::abs(actual.a() - expected.a())});
        }
    }
    EXPECT_LE(maxChannelError, 1.0e-4f);
    const auto overlap = asyncImage.getPixel(56, 56);
    EXPECT_GT(overlap.a(), 0.9f);
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
              workerSubmitCountBeforeAsync + 1);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_async_recorded_overlay_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererDeferredFrameAsyncReadbackCompletesDuringTeardown)
{
    std::promise<QImage> readbackPromise;
    auto readbackFuture = readbackPromise.get_future();
    std::promise<void> callbackEnteredPromise;
    auto callbackEnteredFuture = callbackEnteredPromise.get_future();
    std::promise<void> releaseCallbackPromise;
    auto releaseCallbackFuture = releaseCallbackPromise.get_future().share();
    std::thread releaseCallbackThread;
    std::future_status callbackEnteredStatus = std::future_status::timeout;
    QImage immediateFrame;
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));

    {
        ArtifactTest::HeadlessRenderer renderer;
        if (!renderer.initialize(128, 128)) {
            GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
        }
        renderer.setCanvasSize(128.0f, 128.0f);
        renderer.setClearColor(ArtifactCore::FloatColor(
            0.0f, 0.0f, 0.0f, 0.0f));
        const auto drawDeferredFrame = [&] {
            renderer.clear();
            const ArtifactCore::FloatColor invisible(
                1.0f, 1.0f, 1.0f, 1.0f);
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(
                    -40.0f - static_cast<float>(index) * 3.0f,
                    -40.0f, 2.0f, 2.0f, invisible);
            }
            renderer.drawSolidRect(
                16.0f, 16.0f, 64.0f, 64.0f,
                ArtifactCore::FloatColor(0.95f, 0.1f, 0.1f, 1.0f));
        };

        drawDeferredFrame();
        renderer.flushAndWait();
        immediateFrame = renderer.readbackToImage();
        ASSERT_FALSE(immediateFrame.isNull());

        drawDeferredFrame();

        ASSERT_TRUE(renderer.readbackToImageAsync(
            [&readbackPromise, &callbackEnteredPromise,
             releaseCallbackFuture](const QImage& image) {
                callbackEnteredPromise.set_value();
                releaseCallbackFuture.wait();
                readbackPromise.set_value(image.copy());
            })) << "the bounded async readback ring should accept the cold slot";
        callbackEnteredStatus = callbackEnteredFuture.wait_for(
            std::chrono::seconds(5));
        if (callbackEnteredStatus == std::future_status::ready) {
            releaseCallbackThread = std::thread([&releaseCallbackPromise] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                releaseCallbackPromise.set_value();
            });
        } else {
            releaseCallbackPromise.set_value();
        }
        // Destroy the renderer before waiting for the callback. Its readback
        // pool must join while the staging resources and device are still alive.
    }

    if (releaseCallbackThread.joinable()) releaseCallbackThread.join();
    ASSERT_EQ(callbackEnteredStatus, std::future_status::ready)
        << "readback worker should reach the callback before teardown";
    ASSERT_EQ(readbackFuture.wait_for(std::chrono::seconds(0)),
              std::future_status::ready)
        << "renderer teardown should wait for the in-flight readback worker";
    const QImage frame = readbackFuture.get();
    ASSERT_FALSE(frame.isNull());
    ASSERT_EQ(frame.size(), QSize(128, 128));
    ASSERT_EQ(frame.size(), immediateFrame.size());
    int maximumChannelError = 0;
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const QColor expected = immediateFrame.pixelColor(x, y);
            const QColor actual = frame.pixelColor(x, y);
            maximumChannelError = std::max({
                maximumChannelError,
                std::abs(actual.red() - expected.red()),
                std::abs(actual.green() - expected.green()),
                std::abs(actual.blue() - expected.blue()),
                std::abs(actual.alpha() - expected.alpha())});
        }
    }
    EXPECT_LE(maximumChannelError, 1)
        << "deferred async readback should preserve the synchronous frame";
    const QColor center = frame.pixelColor(48, 48);
    EXPECT_GT(center.red(), 220);
    EXPECT_GT(center.red(), center.green());
    EXPECT_GT(center.red(), center.blue());

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_deferred_async_readback_teardown_%1.png")
            .arg(backend));
    ASSERT_TRUE(frame.save(capturePath, "PNG")) << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererDoesNotOverwriteVideoTextureBeforeDeferredExecution)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    const QUuid videoLayerId = QUuid::createUuid();

    ArtifactCore::ImageF32x4_RGBA redFrame;
    redFrame.resize(4, 4);
    redFrame.fill(ArtifactCore::FloatRGBA(0.95f, 0.05f, 0.05f, 1.0f));
    ArtifactCore::ImageF32x4_RGBA blueFrame;
    blueFrame.resize(4, 4);
    blueFrame.fill(ArtifactCore::FloatRGBA(0.05f, 0.05f, 0.95f, 1.0f));

    const auto drawFrame = [&](Diligent::ITextureView* texture) {
        const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(
                -40.0f - static_cast<float>(index) * 3.0f,
                -40.0f, 2.0f, 2.0f, invisible);
        }
        QMatrix4x4 identity;
        identity.setToIdentity();
        renderer.drawSpriteTransformed(16.0f, 16.0f, 96.0f, 96.0f,
                                       identity, texture, 1.0f);
    };

    renderer.clear();
    auto* redTexture = renderer.textureForVideoFrame(
        redFrame, videoLayerId, 1);
    ASSERT_NE(redTexture, nullptr);
    drawFrame(redTexture);
    renderer.flushAndWait();
    const auto synchronousRedFrame = renderer.readbackToImageF32();
    const auto synchronousRedCenter = synchronousRedFrame.getPixel(64, 64);

    renderer.clear();
    redTexture = renderer.textureForVideoFrame(redFrame, videoLayerId, 1);
    ASSERT_NE(redTexture, nullptr);
    drawFrame(redTexture);
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync());

    // Resolve the next video frame into the same reusable cache slot before
    // the earlier deferred command list has been executed by the owner.
    auto* blueTexture = renderer.textureForVideoFrame(
        blueFrame, videoLayerId, 2);
    ASSERT_NE(blueTexture, nullptr);
    EXPECT_EQ(blueTexture, redTexture);

    renderer.flushAndWait();
    const auto firstFrame = renderer.readbackToImageF32();
    const auto firstCenter = firstFrame.getPixel(64, 64);
    float maxDeferredVsOwnerError = 0.0f;
    for (int y = 0; y < firstFrame.height(); ++y) {
        for (int x = 0; x < firstFrame.width(); ++x) {
            const auto expected = synchronousRedFrame.getPixel(x, y);
            const auto actual = firstFrame.getPixel(x, y);
            maxDeferredVsOwnerError = std::max({
                maxDeferredVsOwnerError,
                std::abs(actual.r() - expected.r()),
                std::abs(actual.g() - expected.g()),
                std::abs(actual.b() - expected.b()),
                std::abs(actual.a() - expected.a())});
        }
    }
    EXPECT_LE(maxDeferredVsOwnerError, 1.0e-4f)
        << "the deferred packet must sample the cached video frame it retained";

    drawFrame(blueTexture);
    renderer.flushAndWait();
    const auto secondFrame = renderer.readbackToImageF32();
    const auto secondCenter = secondFrame.getPixel(64, 64);
    const float secondFrameDifference =
        std::abs(secondCenter.r() - synchronousRedCenter.r()) +
        std::abs(secondCenter.g() - synchronousRedCenter.g()) +
        std::abs(secondCenter.b() - synchronousRedCenter.b());
    EXPECT_GT(secondFrameDifference, 0.5f)
        << "the next packet should sample the updated video frame";

    renderer.clear();
    blueTexture = renderer.textureForVideoFrame(blueFrame, videoLayerId, 2);
    ASSERT_NE(blueTexture, nullptr);
    drawFrame(blueTexture);
    renderer.flushAndWait();

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    EXPECT_TRUE(firstFrame.toQImage().save(
        QDir(captureDirectory).filePath(
            QStringLiteral("artifact_renderer_deferred_video_reuse_first_%1.png")
                .arg(backend)), "PNG"));
    EXPECT_TRUE(secondFrame.toQImage().save(
        QDir(captureDirectory).filePath(
            QStringLiteral("artifact_renderer_deferred_video_reuse_second_%1.png")
                .arg(backend)), "PNG"));
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererDrawsPinnedVideoUploadWithoutCpuImageReference)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    const QUuid videoLayerId = QUuid::createUuid();

    ArtifactCore::ImageF32x4_RGBA sourceFrame;
    sourceFrame.resize(4, 4);
    sourceFrame.fill(ArtifactCore::FloatRGBA(0.95f, 0.05f, 0.05f, 1.0f));
    const auto preparedUpload = ArtifactCore::convertImageForUpload(
        sourceFrame, ArtifactCore::ImageUploadTarget::Rgba8SrgbStraight);
    ASSERT_TRUE(preparedUpload.isValid());

    QMatrix4x4 identity;
    identity.setToIdentity();
    const auto drawTexture = [&](Diligent::ITextureView* texture) {
        const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(
                -40.0f - static_cast<float>(index) * 3.0f,
                -40.0f, 2.0f, 2.0f, invisible);
        }
        renderer.drawSpriteTransformed(16.0f, 16.0f, 96.0f, 96.0f,
                                       identity, texture, 1.0f);
    };

    renderer.clear();
    auto* imageBackedTexture = renderer.textureForVideoFrame(
        sourceFrame, videoLayerId, 1, &preparedUpload);
    ASSERT_NE(imageBackedTexture, nullptr);
    drawTexture(imageBackedTexture);
    renderer.flushAndWait();
    const auto imageBackedFrame = renderer.readbackToImageF32();

    renderer.clear();
    auto* pinnedUploadTexture = renderer.textureForVideoFrame(
        preparedUpload.width, preparedUpload.height, videoLayerId, 1, 7,
        preparedUpload, false);
    ASSERT_NE(pinnedUploadTexture, nullptr);
    drawTexture(pinnedUploadTexture);
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the upload-only textured draw should record through Deferred";
    renderer.flushAndWait();
    const auto pinnedUploadFrame = renderer.readbackToImageF32();

    ASSERT_EQ(pinnedUploadFrame.width(), imageBackedFrame.width());
    ASSERT_EQ(pinnedUploadFrame.height(), imageBackedFrame.height());
    float maximumChannelError = 0.0f;
    for (int y = 0; y < pinnedUploadFrame.height(); ++y) {
        for (int x = 0; x < pinnedUploadFrame.width(); ++x) {
            const auto expected = imageBackedFrame.getPixel(x, y);
            const auto actual = pinnedUploadFrame.getPixel(x, y);
            maximumChannelError = std::max({
                maximumChannelError,
                std::abs(actual.r() - expected.r()),
                std::abs(actual.g() - expected.g()),
                std::abs(actual.b() - expected.b()),
                std::abs(actual.a() - expected.a())});
        }
    }
    EXPECT_LE(maximumChannelError, 1.0e-4f)
        << "upload-only resolution must match the QObject-backed path";

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_pinned_video_upload_%1.png")
            .arg(backend));
    ASSERT_TRUE(pinnedUploadFrame.toQImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererPreservesVideoTextureAcrossDeferredClonerDraw)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128, true)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    if (!renderer.clonerGridPlacementGpuReady()) {
        GTEST_SKIP() << "Diligent backend could not create the Cloner GPU pipelines";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    const QUuid videoLayerId = QUuid::createUuid();
    const auto drawClonerFrame = [&](Diligent::ITextureView* texture) {
        const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
        for (int index = 0; index < 12; ++index) {
            renderer.drawSolidRect(
                -40.0f - static_cast<float>(index) * 3.0f,
                -40.0f, 2.0f, 2.0f, invisible);
        }
        Artifact::ClonerGridPlacementRange range;
        if (!renderer.dispatchClonerGridPlacement(
                2, 1, 1, 40.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.75f, range) ||
            range.count != 3u) {
            return false;
        }
        QMatrix4x4 identity;
        identity.setToIdentity();
        for (std::uint32_t index = 0; index < range.count; ++index) {
            if (!renderer.drawSpriteTransformedClonerGrid(
                    64.0f, 64.0f, 12.0f, 8.0f, identity, identity,
                    texture, 1.0f, range, range.firstIndex + index)) {
                return false;
            }
        }
        return true;
    };
    expectDeferredVideoTextureReuseOrder(
        renderer, videoLayerId,
        QStringLiteral("artifact_renderer_deferred_video_cloner"),
        53, 67, drawClonerFrame);
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererPreservesVideoTextureAcrossDeferredMaskedDraw)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    const QUuid videoLayerId = QUuid::createUuid();
    QImage mask(4, 4, QImage::Format_RGBA8888);
    mask.fill(Qt::white);
    const auto drawMaskedFrame = [&](Diligent::ITextureView* texture) {
        const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(
                -40.0f - static_cast<float>(index) * 3.0f,
                -40.0f, 2.0f, 2.0f, invisible);
        }
        renderer.drawMaskedTextureLocal(
            16.0f, 16.0f, 96.0f, 96.0f, texture, mask);
        return true;
    };
    expectDeferredVideoTextureReuseOrder(
        renderer, videoLayerId,
        QStringLiteral("artifact_renderer_deferred_video_masked"),
        64, 64, drawMaskedFrame);
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererPreservesVideoAndShapeFallbackAtQueueCapacity)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";

    constexpr std::size_t queueCapacity = 16u * 512u * 2u;
    ASSERT_EQ(queueCapacity, 16384u);
    renderer.setCanvasSize(128.0f, 128.0f);
    auto* colorTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenTexture(128, 128));
    auto* depthTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenDepthTexture(128, 128));
    ASSERT_NE(colorTarget, nullptr);
    ASSERT_NE(depthTarget, nullptr);
    renderer.pushRenderTarget(colorTarget, depthTarget);

    ArtifactCore::ImageF32x4_RGBA redFrame;
    redFrame.resize(4, 4);
    redFrame.fill(ArtifactCore::FloatRGBA(0.95f, 0.05f, 0.05f, 1.0f));
    ArtifactCore::ImageF32x4_RGBA blueFrame;
    blueFrame.resize(4, 4);
    blueFrame.fill(ArtifactCore::FloatRGBA(0.05f, 0.05f, 0.95f, 1.0f));
    const QUuid videoLayerId = QUuid::createUuid();

    renderer.clearRenderTarget(colorTarget,
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clearDepthRenderTarget(depthTarget, 1.0f);
    auto* redTexture = renderer.textureForVideoFrame(
        redFrame, videoLayerId, 1);
    ASSERT_NE(redTexture, nullptr);
    const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
    for (std::size_t index = 0; index + 1 < queueCapacity; ++index) {
        renderer.drawSolidRect(
            -40.0f - static_cast<float>(index % 128u) * 3.0f,
            -40.0f, 2.0f, 2.0f, invisible);
    }
    renderer.drawSprite(16.0f, 16.0f, 40.0f, 40.0f, redTexture);
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the full reserved packet queue should record asynchronously";

    auto* blueTexture = renderer.textureForVideoFrame(
        blueFrame, videoLayerId, 2);
    ASSERT_NE(blueTexture, nullptr);
    EXPECT_EQ(blueTexture, redTexture);
    const QImage redImage = renderer.readbackTextureViewToImage(colorTarget);
    ASSERT_FALSE(redImage.isNull());
    const QColor redPixel = redImage.pixelColor(28, 28);
    EXPECT_GT(redPixel.red(), 180);
    EXPECT_LT(redPixel.blue(), 100);

    renderer.clearRenderTarget(colorTarget,
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clearDepthRenderTarget(depthTarget, 1.0f);
    renderer.drawSprite(72.0f, 16.0f, 40.0f, 40.0f, blueTexture);
    for (std::size_t index = 0; index + 1 < queueCapacity; ++index) {
        renderer.drawSolidRect(
            -40.0f - static_cast<float>(index % 128u) * 3.0f,
            -40.0f, 2.0f, 2.0f, invisible);
    }

    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);
    QMatrix4x4 model;
    model.translate(0.0f, 0.0f, -0.5f);
    const std::vector<Artifact::Detail::float2> triangle{
        {-0.8f, -0.8f}, {0.8f, -0.8f}, {0.0f, 0.8f}};
    const auto workerSubmitsBeforeShape = renderer.deferredWorkerSubmitCount();
    renderer.draw3DShape(
        triangle, model, ArtifactCore::FloatColor(0.1f, 0.85f, 0.2f, 1.0f),
        1.0f, true);
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitsBeforeShape + 1)
        << "queue saturation must drain the full eligible batch before the owner fallback";
    renderer.reset3DCameraMatrices();
    renderer.flushAndWait();

    const QImage blueImage = renderer.readbackTextureViewToImage(colorTarget);
    ASSERT_FALSE(blueImage.isNull());
    const QColor bluePixel = blueImage.pixelColor(92, 28);
    EXPECT_GT(bluePixel.blue(), 180);
    EXPECT_LT(bluePixel.red(), 100);
    const QColor shapePixel = blueImage.pixelColor(64, 64);
    EXPECT_GT(shapePixel.green(), 130);
    EXPECT_LT(shapePixel.red(), 100);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    ASSERT_TRUE(redImage.save(QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_video_queue_capacity_first_%1.png")
            .arg(backend)), "PNG"));
    ASSERT_TRUE(blueImage.save(QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_video_queue_capacity_fallback_%1.png")
            .arg(backend)), "PNG"));
    renderer.reset3DCameraMatrices();
    renderer.popRenderTarget();
    renderer.destroyOffscreenTexture(colorTarget);
    renderer.destroyOffscreenTexture(depthTarget);
}

TEST(ClonerGpuRenderContractTest,
     DeferredComputeFxaaMatchesImmediateRenderedFrame)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(64, 64)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }

    auto device = renderer.device();
    auto immediate = renderer.immediateContext();
    ASSERT_NE(device, nullptr);
    ASSERT_NE(immediate, nullptr);

    constexpr Diligent::Uint32 width = 64;
    constexpr Diligent::Uint32 height = 64;
    Artifact::RenderPipeline pipeline;
    ASSERT_TRUE(pipeline.initialize(device.RawPtr(), width, height,
                                    Artifact::RenderConfig::LinearColorFormat));

    // High-contrast diagonal and horizontal edges make the FXAA dispatch do
    // visible work while keeping all input resident on the GPU.
    std::vector<float> pixels(static_cast<std::size_t>(width) * height * 4u);
    for (Diligent::Uint32 y = 0; y < height; ++y) {
        for (Diligent::Uint32 x = 0; x < width; ++x) {
            const bool bright = x >= y || y >= 40;
            const float value = bright ? 1.0f : 0.0f;
            const auto offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            pixels[offset + 0u] = value;
            pixels[offset + 1u] = value * 0.35f;
            pixels[offset + 2u] = 1.0f - value;
            pixels[offset + 3u] = 1.0f;
        }
    }

    Diligent::TextureDesc sourceDesc;
    sourceDesc.Name = "Deferred FXAA parity source";
    sourceDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    sourceDesc.Width = width;
    sourceDesc.Height = height;
    sourceDesc.MipLevels = 1;
    sourceDesc.Format = Artifact::RenderConfig::LinearColorFormat;
    sourceDesc.Usage = Diligent::USAGE_IMMUTABLE;
    sourceDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData sourceSubresource;
    sourceSubresource.pData = pixels.data();
    sourceSubresource.Stride = width * 4u * sizeof(float);
    Diligent::TextureData sourceInitialData;
    sourceInitialData.pSubResources = &sourceSubresource;
    sourceInitialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> sourceTexture;
    device->CreateTexture(sourceDesc, &sourceInitialData, &sourceTexture);
    ASSERT_NE(sourceTexture, nullptr);
    auto* sourceSRV = sourceTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(sourceSRV, nullptr);

    // Build the FXAA PSO/SRB on the owner context before the worker records.
    ASSERT_TRUE(pipeline.applyFastApproximateAntiAliasing(
        immediate.RawPtr(), sourceSRV, pipeline.tempUAV()));
    renderer.flushAndWait();
    const QImage immediateImage =
        renderer.readbackTextureViewToImage(pipeline.tempSRV());
    ASSERT_FALSE(immediateImage.isNull());

    struct DeferredFxaaRequest {
        Artifact::RenderPipeline* pipeline = nullptr;
        Diligent::ITextureView* source = nullptr;
        Diligent::ITextureView* destination = nullptr;
    } request{&pipeline, sourceSRV, pipeline.accumUAV()};
    const auto recordFxaa = +[](void* opaque,
                                Diligent::IDeviceContext* deferredContext) {
        auto* deferredRequest = static_cast<DeferredFxaaRequest*>(opaque);
        return deferredRequest && deferredRequest->pipeline && deferredContext &&
               deferredRequest->pipeline->applyFastApproximateAntiAliasing(
                   deferredContext, deferredRequest->source,
                   deferredRequest->destination);
    };
    ASSERT_TRUE(renderer.startDeferredContextWork(&request, recordFxaa));
    ASSERT_TRUE(renderer.executeDeferredContextWork());
    renderer.flushAndWait();
    const QImage deferredImage =
        renderer.readbackTextureViewToImage(pipeline.accumSRV());
    ASSERT_FALSE(deferredImage.isNull());
    ASSERT_EQ(deferredImage.size(), immediateImage.size());

    std::size_t differingPixels = 0;
    int maximumChannelDifference = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QColor immediatePixel = immediateImage.pixelColor(x, y);
            const QColor deferredPixel = deferredImage.pixelColor(x, y);
            const int channelDifference = std::max({
                std::abs(immediatePixel.red() - deferredPixel.red()),
                std::abs(immediatePixel.green() - deferredPixel.green()),
                std::abs(immediatePixel.blue() - deferredPixel.blue()),
                std::abs(immediatePixel.alpha() - deferredPixel.alpha())});
            maximumChannelDifference =
                std::max(maximumChannelDifference, channelDifference);
            differingPixels += channelDifference != 0 ? 1u : 0u;
        }
    }
    EXPECT_EQ(differingPixels, 0u)
        << "maximum 8-bit channel difference=" << maximumChannelDifference;

    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("deferred_fxaa_frame_%1.png").arg(backend));
    ASSERT_TRUE(deferredImage.save(capturePath, "PNG")) << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererDefersRetainedBillboardPacketWithCameraState)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);

    const auto createWhiteTexture = [&]() {
        constexpr std::array<std::uint8_t, 4> texel{255, 255, 255, 255};
        Diligent::TextureDesc desc;
        desc.Name = "ArtifactIRenderer deferred billboard contract";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData subresource;
        subresource.pData = texel.data();
        subresource.Stride = texel.size();
        Diligent::TextureData initialData;
        initialData.pSubResources = &subresource;
        initialData.NumSubresources = 1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        renderer.device()->CreateTexture(desc, &initialData, &texture);
        return texture;
    };
    const auto drawBillboardBatch = [&](Diligent::ITextureView* textureView) {
        const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                                   -48.0f, 2.0f, 2.0f, invisible);
        }
        renderer.draw3DBillboardQuad(
            {0.0f, 0.0f, 0.0f}, {1.8f, 1.8f}, textureView,
            ArtifactCore::FloatColor(0.95f, 0.25f, 0.05f, 1.0f));
    };
    const ArtifactCore::FloatColor clearColor(0.0f, 0.0f, 0.0f, 0.0f);

    renderer.setClearColor(clearColor);
    renderer.clear();
    auto immediateTexture = createWhiteTexture();
    ASSERT_NE(immediateTexture, nullptr);
    drawBillboardBatch(immediateTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
    renderer.flushAndWait();
    const auto immediateImage = renderer.readbackToImageF32();

    renderer.clear();
    auto deferredTexture = createWhiteTexture();
    ASSERT_NE(deferredTexture, nullptr);
    drawBillboardBatch(deferredTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
    deferredTexture = nullptr;
    const quint64 workerSubmitCountBefore =
        renderer.deferredWorkerSubmitCount();
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the retained BillboardPkt should be accepted by the 3D worker recorder";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);

    const auto deferredImage = renderer.readbackToImageF32();
    ASSERT_EQ(deferredImage.width(), immediateImage.width());
    ASSERT_EQ(deferredImage.height(), immediateImage.height());
    float maxChannelError = 0.0f;
    for (int y = 0; y < deferredImage.height(); ++y) {
        for (int x = 0; x < deferredImage.width(); ++x) {
            const auto expected = immediateImage.getPixel(x, y);
            const auto actual = deferredImage.getPixel(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(actual.r() - expected.r()),
                std::abs(actual.g() - expected.g()),
                std::abs(actual.b() - expected.b()),
                std::abs(actual.a() - expected.a())});
        }
    }
    EXPECT_LE(maxChannelError, 1.0e-4f);
    const auto center = deferredImage.getPixel(64, 64);
    EXPECT_GT(center.r(), 0.8f);
    EXPECT_GT(center.a(), 0.9f);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_deferred_billboard_%1.png").arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     DeferredBillboardOffscreenTargetSurvivesImmediate3DCardBarrier)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";
    renderer.setCanvasSize(128.0f, 128.0f);
    auto* colorTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenTexture(128, 128));
    auto* depthTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenDepthTexture(128, 128));
    ASSERT_NE(colorTarget, nullptr);
    ASSERT_NE(depthTarget, nullptr);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);

    constexpr std::array<std::uint8_t, 4> texel{255, 255, 255, 255};
    Diligent::TextureDesc textureDesc;
    textureDesc.Name = "Deferred offscreen Billboard texture";
    textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    textureDesc.Usage = Diligent::USAGE_IMMUTABLE;
    textureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData subresource;
    subresource.pData = texel.data();
    subresource.Stride = static_cast<Diligent::Uint32>(texel.size());
    Diligent::TextureData initialData;
    initialData.pSubResources = &subresource;
    initialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    renderer.device()->CreateTexture(textureDesc, &initialData, &texture);
    ASSERT_NE(texture, nullptr);

    const auto render = [&](bool requestDeferred) {
        renderer.set3DCameraMatrices(view, projection);
        renderer.pushRenderTarget(colorTarget, depthTarget);
        const ArtifactCore::FloatColor transparent(0.0f, 0.0f, 0.0f, 0.0f);
        renderer.clearRenderTarget(transparent);
        renderer.clearDepthRenderTarget(depthTarget, 1.0f);
        const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                                   -48.0f, 2.0f, 2.0f, invisible);
        }
        renderer.draw3DBillboardQuad(
            {0.0f, 0.0f, 0.0f}, {1.8f, 1.8f},
            texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE),
            ArtifactCore::FloatColor(0.95f, 0.25f, 0.05f, 1.0f));

        bool workerSelected = false;
        quint64 workerSubmitCountBefore = 0;
        if (requestDeferred) {
            workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
            workerSelected = renderer.flushDeferredPacketsAsync();
            EXPECT_TRUE(workerSelected)
                << "the offscreen Billboard batch should enter deferred recording";
        } else {
            renderer.flushAndWait();
        }

        // This depth-tested owner draw is an ordering barrier after the
        // Billboard batch and must keep the active offscreen RTV/DSV bound.
        QMatrix4x4 cardModel;
        cardModel.translate(0.0f, 0.0f, -0.5f);
        renderer.draw3DCard(QRectF(-0.7, -0.7, 1.4, 1.4), cardModel,
                            ArtifactCore::FloatColor(0.05f, 0.15f, 0.95f, 1.0f),
                            1.0f, true);
        if (requestDeferred) {
            // Keep the second batch eligible so CardPkt itself is recorded by
            // the worker after the Billboard barrier. These packets are fully
            // off-target and must not alter the captured frame.
            const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                                       -48.0f, 2.0f, 2.0f, invisible);
            }
            renderer.reset3DCameraMatrices();
        }
        renderer.flushAndWait();
        if (requestDeferred && workerSelected) {
            EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
                      workerSubmitCountBefore + 2)
                << "Billboard barrier and CardPkt batch should both record on worker";
        }
        const QImage image = renderer.readbackTextureViewToImage(colorTarget);
        if (!requestDeferred) renderer.reset3DCameraMatrices();
        renderer.popRenderTarget();
        return image;
    };

    const QImage immediateImage = render(false);
    const QImage deferredImage = render(true);
    ASSERT_FALSE(immediateImage.isNull());
    ASSERT_FALSE(deferredImage.isNull());
    ASSERT_EQ(deferredImage.size(), immediateImage.size());
    int maxChannelError = 0;
    for (int y = 0; y < immediateImage.height(); ++y) {
        for (int x = 0; x < immediateImage.width(); ++x) {
            const QColor expected = immediateImage.pixelColor(x, y);
            const QColor actual = deferredImage.pixelColor(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(actual.red() - expected.red()),
                std::abs(actual.green() - expected.green()),
                std::abs(actual.blue() - expected.blue()),
                std::abs(actual.alpha() - expected.alpha())});
        }
    }
    EXPECT_LE(maxChannelError, 1);
    const QColor center = deferredImage.pixelColor(64, 64);
    EXPECT_GT(center.blue(), 200);
    EXPECT_LT(center.red(), 100);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_deferred_billboard_offscreen_%1.png")
            .arg(backend));
    ASSERT_TRUE(deferredImage.save(capturePath, "PNG"))
        << capturePath.toStdString();
    renderer.destroyOffscreenTexture(colorTarget);
    renderer.destroyOffscreenTexture(depthTarget);
}

TEST(ClonerGpuRenderContractTest,
     ProjectorCardFallsBackToOwnerAfterDeferredQueueBarrier)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";

    renderer.setCanvasSize(128.0f, 128.0f);
    auto* colorTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenTexture(128, 128));
    auto* depthTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenDepthTexture(128, 128));
    ASSERT_NE(colorTarget, nullptr);
    ASSERT_NE(depthTarget, nullptr);

    constexpr std::array<std::uint8_t, 4> texel{255, 255, 255, 255};
    Diligent::TextureDesc textureDesc;
    textureDesc.Name = "Deferred projector fallback texture";
    textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    textureDesc.Usage = Diligent::USAGE_IMMUTABLE;
    textureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData subresource;
    subresource.pData = texel.data();
    subresource.Stride = static_cast<Diligent::Uint32>(texel.size());
    Diligent::TextureData initialData;
    initialData.pSubResources = &subresource;
    initialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    renderer.device()->CreateTexture(textureDesc, &initialData, &texture);
    ASSERT_NE(texture, nullptr);

    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);

    renderer.pushRenderTarget(colorTarget, depthTarget);
    renderer.clearRenderTarget(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clearDepthRenderTarget(depthTarget, 1.0f);
    const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
    for (int index = 0; index < 16; ++index) {
        renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                               -48.0f, 2.0f, 2.0f, invisible);
    }

    QMatrix4x4 projectorView;
    QMatrix4x4 projectorProjection;
    projectorProjection.setToIdentity();
    renderer.setProjectorSource(
        texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
    renderer.setProjectorMatrices(projectorView, projectorProjection);
    renderer.setProjectorEnabled(true);
    const auto workerSubmitsBeforeCard = renderer.deferredWorkerSubmitCount();
    QMatrix4x4 cardModel;
    cardModel.translate(0.0f, 0.0f, -0.5f);
    renderer.draw3DCard(QRectF(-0.7, -0.7, 1.4, 1.4), cardModel,
                        ArtifactCore::FloatColor(0.05f, 0.15f, 0.95f, 1.0f),
                        1.0f, true);
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitsBeforeCard + 1)
        << "the preceding queue should be deferred; projector Card stays owner-side";
    renderer.resetProjector();

    for (int index = 0; index < 15; ++index) {
        renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                               -48.0f, 2.0f, 2.0f, invisible);
    }
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitsBeforeCard + 1)
        << "the projector Card must not be re-queued into the later packet batch";

    const QImage image = renderer.readbackTextureViewToImage(colorTarget);
    ASSERT_FALSE(image.isNull());
    const QColor center = image.pixelColor(64, 64);
    EXPECT_GT(center.blue(), 200);
    EXPECT_LT(center.red(), 100);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_projector_card_fallback_%1.png")
            .arg(requestedBackend));
    ASSERT_TRUE(image.save(capturePath, "PNG")) << capturePath.toStdString();
    renderer.popRenderTarget();
    renderer.destroyOffscreenTexture(colorTarget);
    renderer.destroyOffscreenTexture(depthTarget);
}

TEST(ClonerGpuRenderContractTest,
     DeferredTexturedCardRetainsTextureAndPreservesTwoPassImage)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";

    renderer.setCanvasSize(128.0f, 128.0f);
    auto* colorTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenTexture(128, 128));
    auto* depthTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenDepthTexture(128, 128));
    ASSERT_NE(colorTarget, nullptr);
    ASSERT_NE(depthTarget, nullptr);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);

    const auto render = [&](bool requestDeferred) {
        renderer.set3DCameraMatrices(view, projection);
        constexpr std::array<std::uint8_t, 4> texel{235, 58, 24, 255};
        Diligent::TextureDesc textureDesc;
        textureDesc.Name = "Deferred textured card retained texture";
        textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        textureDesc.Width = 1;
        textureDesc.Height = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        textureDesc.Usage = Diligent::USAGE_IMMUTABLE;
        textureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData subresource;
        subresource.pData = texel.data();
        subresource.Stride = static_cast<Diligent::Uint32>(texel.size());
        Diligent::TextureData initialData;
        initialData.pSubResources = &subresource;
        initialData.NumSubresources = 1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        renderer.device()->CreateTexture(textureDesc, &initialData, &texture);
        EXPECT_NE(texture, nullptr);

        renderer.pushRenderTarget(colorTarget, depthTarget);
        renderer.clearRenderTarget(
            ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
        renderer.clearDepthRenderTarget(depthTarget, 1.0f);
        QMatrix4x4 cardModel;
        cardModel.translate(0.0f, 0.0f, -0.5f);
        renderer.draw3DTexturedCard(
            QRectF(-0.7, -0.7, 1.4, 1.4), cardModel,
            texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE),
            1.0f);
        // The packet must retain the SRV until both card passes execute.
        texture = nullptr;
        if (requestDeferred) {
            const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                                       -48.0f, 2.0f, 2.0f, invisible);
            }
            // Mirror Scoped3DLayerCamera: its camera is reset before the later
            // packet flush, so TexturedCardPkt must carry its own snapshot.
            renderer.reset3DCameraMatrices();
        }
        const auto workerSubmitsBeforeFlush =
            renderer.deferredWorkerSubmitCount();
        renderer.flushAndWait();
        EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
                  workerSubmitsBeforeFlush + (requestDeferred ? 1 : 0));
        const QImage image = renderer.readbackTextureViewToImage(colorTarget);
        if (!requestDeferred) renderer.reset3DCameraMatrices();
        renderer.popRenderTarget();
        return image;
    };

    const QImage ownerImage = render(false);
    const QImage deferredImage = render(true);
    ASSERT_FALSE(ownerImage.isNull());
    ASSERT_FALSE(deferredImage.isNull());
    ASSERT_EQ(deferredImage.size(), ownerImage.size());
    int maxChannelError = 0;
    for (int y = 0; y < ownerImage.height(); ++y) {
        for (int x = 0; x < ownerImage.width(); ++x) {
            const QColor expected = ownerImage.pixelColor(x, y);
            const QColor actual = deferredImage.pixelColor(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(actual.red() - expected.red()),
                std::abs(actual.green() - expected.green()),
                std::abs(actual.blue() - expected.blue()),
                std::abs(actual.alpha() - expected.alpha())});
        }
    }
    EXPECT_LE(maxChannelError, 1);
    const QColor center = deferredImage.pixelColor(64, 64);
    EXPECT_GT(center.red(), 200);
    EXPECT_LT(center.blue(), 100);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_deferred_textured_card_%1.png")
            .arg(requestedBackend));
    ASSERT_TRUE(deferredImage.save(capturePath, "PNG"))
        << capturePath.toStdString();
    renderer.destroyOffscreenTexture(colorTarget);
    renderer.destroyOffscreenTexture(depthTarget);
}

TEST(ClonerGpuRenderContractTest,
     DeferredShapeTrianglesRetainCameraAndPreserveOffscreenImage)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";

    renderer.setCanvasSize(128.0f, 128.0f);
    auto* colorTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenTexture(128, 128));
    auto* depthTarget = static_cast<Diligent::ITextureView*>(
        renderer.createOffscreenDepthTexture(128, 128));
    ASSERT_NE(colorTarget, nullptr);
    ASSERT_NE(depthTarget, nullptr);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    const std::vector<Artifact::Detail::float2> triangle{
        {-0.8f, -0.8f}, {0.8f, -0.8f}, {0.0f, 0.8f}};
    const auto render = [&](bool requestDeferred) {
        renderer.set3DCameraMatrices(view, projection);
        renderer.pushRenderTarget(colorTarget, depthTarget);
        renderer.clearRenderTarget(
            ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
        renderer.clearDepthRenderTarget(depthTarget, 1.0f);
        if (requestDeferred) {
            const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
            for (int index = 0; index < 15; ++index) {
                renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                                       -48.0f, 2.0f, 2.0f, invisible);
            }
        }
        QMatrix4x4 model;
        model.translate(0.0f, 0.0f, -0.5f);
        renderer.draw3DShape(
            triangle, model, ArtifactCore::FloatColor(0.1f, 0.25f, 0.95f, 1.0f),
            1.0f, true);
        if (requestDeferred) {
            // Composition resets its scoped camera before the packet queue is
            // flushed; ShapeTrianglesPkt must preserve the draw-time camera.
            renderer.reset3DCameraMatrices();
        }
        const auto workerSubmitsBeforeFlush =
            renderer.deferredWorkerSubmitCount();
        renderer.flushAndWait();
        EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
                  workerSubmitsBeforeFlush + (requestDeferred ? 1 : 0));
        const QImage image = renderer.readbackTextureViewToImage(colorTarget);
        if (!requestDeferred) renderer.reset3DCameraMatrices();
        renderer.popRenderTarget();
        return image;
    };

    const QImage ownerImage = render(false);
    const QImage deferredImage = render(true);
    ASSERT_FALSE(ownerImage.isNull());
    ASSERT_FALSE(deferredImage.isNull());
    ASSERT_EQ(deferredImage.size(), ownerImage.size());
    int maxChannelError = 0;
    for (int y = 0; y < ownerImage.height(); ++y) {
        for (int x = 0; x < ownerImage.width(); ++x) {
            const QColor expected = ownerImage.pixelColor(x, y);
            const QColor actual = deferredImage.pixelColor(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(actual.red() - expected.red()),
                std::abs(actual.green() - expected.green()),
                std::abs(actual.blue() - expected.blue()),
                std::abs(actual.alpha() - expected.alpha())});
        }
    }
    EXPECT_LE(maxChannelError, 1);
    const QColor center = deferredImage.pixelColor(64, 64);
    EXPECT_GT(center.blue(), 200);
    EXPECT_LT(center.red(), 100);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_deferred_shape_triangles_%1.png")
            .arg(requestedBackend));
    ASSERT_TRUE(deferredImage.save(capturePath, "PNG"))
        << capturePath.toStdString();
    renderer.destroyOffscreenTexture(colorTarget);
    renderer.destroyOffscreenTexture(depthTarget);
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererDefersSingleLargeMpmSurfaceByBoundedWork)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    const ArtifactCore::FloatColor clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const ArtifactCore::FloatColor surfaceColor(0.1f, 0.9f, 0.25f, 1.0f);
    const auto drawMaximumSurface = [&] {
        ArtifactCore::ArtifactArray<Artifact::MpmSurfaceVertex> vertices;
        appendMaximumMpmTestSurface(vertices);
        EXPECT_TRUE(renderer.drawMpmSurface(vertices, surfaceColor));
    };

    renderer.setClearColor(clearColor);
    renderer.clear();
    drawMaximumSurface();
    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    EXPECT_TRUE(renderer.flushDeferredPacketsAsync())
        << "one maximum-size MPM packet should meet the bounded worker workload threshold";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);

    const auto workerImage = renderer.readbackToImageF32();
    const auto surfacePixel = workerImage.getPixel(22, 18);
    EXPECT_GT(surfacePixel.a(), 0.8f);
    EXPECT_GT(surfacePixel.g(), 0.75f);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto workerPath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_mpm_single_packet_worker_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(workerPath, "PNG"))
        << workerPath.toStdString();

    renderer.clear();
    ArtifactCore::ArtifactArray<Artifact::MpmSurfaceVertex> smallVertices;
    smallVertices.reserve(3);
    smallVertices.append({12.0f, 12.0f, 1.0f});
    smallVertices.append({32.0f, 12.0f, 1.0f});
    smallVertices.append({22.0f, 32.0f, 1.0f});
    ASSERT_TRUE(renderer.drawMpmSurface(smallVertices, surfaceColor));
    EXPECT_FALSE(renderer.flushDeferredPacketsAsync())
        << "a single small MPM packet should stay on the owner path";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);
    const auto smallImage = renderer.readbackToImageF32();
    const auto smallSurfacePixel = smallImage.getPixel(22, 18);
    EXPECT_GT(smallSurfacePixel.a(), 0.8f);
    EXPECT_GT(smallSurfacePixel.g(), 0.75f);
    const auto smallPath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_mpm_single_packet_owner_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(smallPath, "PNG"))
        << smallPath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererDefersSingleLargeParticleUploadByBoundedWork)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 128.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);

    constexpr std::size_t kParticleCount = 65'536;
    ArtifactCore::ParticleRenderData particleData;
    particleData.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    particleData.particles.reserve(kParticleCount);
    for (std::size_t index = 0; index < kParticleCount; ++index) {
        particleData.particles.emplace_back();
        auto& particle = particleData.particles.back();
        particle.px = 1000.0f;
        particle.a = 0.0f;
        particle.size = 0.0f;
    }
    auto& visibleParticle = particleData.particles.front();
    visibleParticle.px = 0.0f;
    visibleParticle.r = 1.0f;
    visibleParticle.g = 0.1f;
    visibleParticle.b = 0.05f;
    visibleParticle.a = 1.0f;
    visibleParticle.size = 0.35f;
    visibleParticle.lifetime = 1.0f;

    renderer.clear();
    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    renderer.drawParticles(std::move(particleData));
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "a 65,536-particle upload should meet the bounded worker workload threshold";
    renderer.drawSolidRect(100.0f, 8.0f, 20.0f, 20.0f,
                           ArtifactCore::FloatColor(0.1f, 0.2f, 0.95f, 1.0f));
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);

    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), 128);
    ASSERT_EQ(image.height(), 128);
    const auto particlePixel = image.getPixel(64, 64);
    EXPECT_GT(particlePixel.a(), 0.8f);
    EXPECT_GT(particlePixel.r(), 0.7f);
    EXPECT_LT(particlePixel.g(), 0.5f);
    const auto overlayPixel = image.getPixel(108, 16);
    EXPECT_GT(overlayPixel.b(), 0.8f);
    EXPECT_LT(overlayPixel.r(), 0.4f);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_particle_payload_worker_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();

    renderer.clear();
    ArtifactCore::ParticleRenderData repeatedParticleData;
    repeatedParticleData.options.blend =
        ArtifactCore::ParticleBlendPolicy::Alpha;
    repeatedParticleData.particles.reserve(kParticleCount);
    for (std::size_t index = 0; index < kParticleCount; ++index) {
        repeatedParticleData.particles.emplace_back();
        auto& particle = repeatedParticleData.particles.back();
        particle.px = 1000.0f;
        particle.a = 0.0f;
        particle.size = 0.0f;
    }
    auto& repeatedVisibleParticle = repeatedParticleData.particles.front();
    repeatedVisibleParticle.px = 0.0f;
    repeatedVisibleParticle.r = 1.0f;
    repeatedVisibleParticle.g = 0.1f;
    repeatedVisibleParticle.b = 0.05f;
    repeatedVisibleParticle.a = 1.0f;
    repeatedVisibleParticle.size = 0.35f;
    repeatedVisibleParticle.lifetime = 1.0f;
    const quint64 workerSubmitCountBeforeRepeat =
        renderer.deferredWorkerSubmitCount();
    renderer.drawParticles(std::move(repeatedParticleData));
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "a repeated 65,536-particle upload should reuse the worker path";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
              workerSubmitCountBeforeRepeat + 1);
    const auto repeatedImage = renderer.readbackToImageF32();
    EXPECT_GT(repeatedImage.getPixel(64, 64).a(), 0.8f);
    EXPECT_GT(repeatedImage.getPixel(64, 64).r(), 0.7f);
    const auto repeatedPath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_particle_payload_worker_repeat_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(repeatedPath, "PNG"))
        << repeatedPath.toStdString();

    renderer.clear();
    ArtifactCore::ParticleRenderData smallParticleData;
    smallParticleData.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    smallParticleData.particles.emplace_back();
    auto& smallParticle = smallParticleData.particles.back();
    smallParticle.r = 1.0f;
    smallParticle.g = 0.1f;
    smallParticle.b = 0.05f;
    smallParticle.a = 1.0f;
    smallParticle.size = 0.35f;
    smallParticle.lifetime = 1.0f;
    const quint64 workerSubmitCountBeforeSmall =
        renderer.deferredWorkerSubmitCount();
    renderer.drawParticles(std::move(smallParticleData));
    EXPECT_FALSE(renderer.flushDeferredPacketsAsync())
        << "a single small Particle packet should stay on the owner path";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBeforeSmall);

    const auto smallImage = renderer.readbackToImageF32();
    const auto smallParticlePixel = smallImage.getPixel(64, 64);
    EXPECT_GT(smallParticlePixel.a(), 0.8f);
    EXPECT_GT(smallParticlePixel.r(), 0.7f);
    const auto ownerPath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_particle_payload_owner_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(ownerPath, "PNG"))
        << ownerPath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererDefersWarmGlyphAndTransformedTextWithPreparedAtlasData)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(192, 96)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";
    renderer.setCanvasSize(192.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
    const QDir testSourceDirectory(
        QFileInfo(QString::fromLocal8Bit(__FILE__)).absolutePath());
    // Headless Qt can otherwise map every character to its replacement glyph.
    const QString bundledFontPath = testSourceDirectory.filePath(
        QStringLiteral("../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    const int bundledFontId = QFontDatabase::addApplicationFont(bundledFontPath);
    ASSERT_NE(bundledFontId, -1) << bundledFontPath.toStdString();
    const auto bundledFamilies =
        QFontDatabase::applicationFontFamilies(bundledFontId);
    ASSERT_FALSE(bundledFamilies.isEmpty());
    const QFont font(bundledFamilies.front(), 24);
    const auto distinctRawFontSilhouettes = [&font](const QString& text) {
        const QRawFont rawFont = QRawFont::fromFont(font, QFontDatabase::Any);
        if (!rawFont.isValid()) return std::pair<std::size_t, std::size_t>{0, 0};

        std::array<std::uint64_t, 32> signatures{};
        std::size_t signatureCount = 0;
        std::array<quint32, 32> glyphIds{};
        std::size_t glyphIdCount = 0;
        for (const QChar character : text) {
            const auto glyphIndices = rawFont.glyphIndexesForString(
                QString(character));
            if (glyphIndices.isEmpty() || glyphIndices.front() == 0) continue;
            bool glyphAlreadySeen = false;
            for (std::size_t index = 0; index < glyphIdCount; ++index) {
                glyphAlreadySeen |= glyphIds[index] == glyphIndices.front();
            }
            if (!glyphAlreadySeen && glyphIdCount < glyphIds.size()) {
                glyphIds[glyphIdCount++] = glyphIndices.front();
            }
            const QImage bitmap = rawFont.alphaMapForGlyph(
                glyphIndices.front(), QRawFont::PixelAntialiasing);
            if (bitmap.isNull() || bitmap.width() <= 0 ||
                bitmap.height() <= 0) {
                continue;
            }

            std::uint64_t signature = 1469598103934665603ull;
            const auto mix = [&signature](std::uint64_t value) {
                signature ^= value;
                signature *= 1099511628211ull;
            };
            mix(static_cast<std::uint64_t>(bitmap.width()));
            mix(static_cast<std::uint64_t>(bitmap.height()));
            for (int y = 0; y < bitmap.height(); ++y) {
                for (int x = 0; x < bitmap.width(); ++x) {
                    const auto* row = bitmap.constScanLine(y);
                    const int alpha = bitmap.format() == QImage::Format_Alpha8
                                          ? row[x]
                                          : qGray(bitmap.pixel(x, y));
                    mix(alpha > 127 ? 1u : 0u);
                }
            }

            bool alreadySeen = false;
            for (std::size_t index = 0; index < signatureCount; ++index) {
                alreadySeen |= signatures[index] == signature;
            }
            if (!alreadySeen && signatureCount < signatures.size()) {
                signatures[signatureCount++] = signature;
            }
        }
        return std::pair{signatureCount, glyphIdCount};
    };
    const auto ownerFontGlyphs = distinctRawFontSilhouettes(
        QStringLiteral("Deferred"));
    const auto transformedFontGlyphs = distinctRawFontSilhouettes(
        QStringLiteral("Transformed"));
    EXPECT_GE(ownerFontGlyphs.second, 4u)
        << "Qt mapped too few distinct glyph IDs for the selected font";
    EXPECT_GE(ownerFontGlyphs.first, 4u)
        << "the selected Qt font must provide distinct glyph masks";
    EXPECT_GE(transformedFontGlyphs.second, 4u)
        << "Qt mapped too few distinct transformed glyph IDs";
    EXPECT_GE(transformedFontGlyphs.first, 4u)
        << "the selected Qt font must provide distinct transformed glyph masks";
    const ArtifactCore::FloatColor textColor(0.1f, 1.0f, 0.2f, 1.0f);
    QMatrix4x4 textTransform;
    textTransform.translate(6.0f, 0.0f, 0.0f);
    const auto drawEligibleBatchWithText = [&] {
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                   -40.0f, 2.0f, 2.0f, invisible);
        }
        renderer.drawText(QRectF(8.0, 8.0, 112.0, 40.0),
                          QStringLiteral("Deferred"), font, textColor);
        renderer.drawTextTransformed(
            QRectF(8.0, 50.0, 176.0, 40.0), QStringLiteral("Transformed"),
            font, textColor, textTransform);
    };
    const auto drawColdGlyphSeedBatch = [&] {
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                   -40.0f, 2.0f, 2.0f, invisible);
        }
        renderer.drawText(QRectF(8.0, 8.0, 112.0, 40.0),
                          QStringLiteral("Seed"), font, textColor);
    };

    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    renderer.clear();
    drawColdGlyphSeedBatch();
    EXPECT_FALSE(renderer.flushDeferredPacketsAsync())
        << "a cold glyph atlas must stay on the owner path";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore);

    renderer.clear();
    drawEligibleBatchWithText();
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "new glyphs must be rasterized and uploaded on the owner before worker recording";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);

    const auto deferredImage = renderer.readbackToImageF32();
    renderer.clear();
    drawEligibleBatchWithText();
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);
    const auto ownerImage = renderer.readbackToImageF32();

    const auto& image = deferredImage;
    const auto& immediateImage = ownerImage;
    ASSERT_EQ(image.width(), immediateImage.width());
    ASSERT_EQ(image.height(), immediateImage.height());
    std::size_t greenTextPixels = 0;
    std::size_t greenTransformedTextPixels = 0;
    const auto distinctGlyphSilhouettes = [&image](int firstY, int lastY) {
        const auto components = findVisibleComponents(image);
        std::array<std::uint64_t, 32> signatures{};
        std::size_t signatureCount = 0;
        for (const auto& component : components) {
            if (component.top < firstY || component.bottom >= lastY) continue;

            std::uint64_t signature = 1469598103934665603ull;
            const auto mix = [&signature](std::uint64_t value) {
                signature ^= value;
                signature *= 1099511628211ull;
            };
            mix(static_cast<std::uint64_t>(component.right - component.left + 1));
            mix(static_cast<std::uint64_t>(component.bottom - component.top + 1));
            for (int y = component.top; y <= component.bottom; ++y) {
                for (int x = component.left; x <= component.right; ++x) {
                    mix(image.getPixel(x, y).a() > 0.5f ? 1u : 0u);
                }
            }

            bool alreadySeen = false;
            for (std::size_t index = 0; index < signatureCount; ++index) {
                alreadySeen |= signatures[index] == signature;
            }
            if (!alreadySeen && signatureCount < signatures.size()) {
                signatures[signatureCount++] = signature;
            }
        }
        return signatureCount;
    };
    float maxChannelError = 0.0f;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.getPixel(x, y);
            if (pixel.a() > 0.5f && pixel.g() > 0.5f && pixel.g() > pixel.r()) {
                if (y >= 8 && y < 48) ++greenTextPixels;
                if (y >= 50 && y < 96) ++greenTransformedTextPixels;
            }
            const auto expected = immediateImage.getPixel(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(pixel.r() - expected.r()),
                std::abs(pixel.g() - expected.g()),
                std::abs(pixel.b() - expected.b()),
                std::abs(pixel.a() - expected.a())});
        }
    }
    EXPECT_GT(greenTextPixels, 0u);
    EXPECT_GT(greenTransformedTextPixels, 0u);
    EXPECT_GE(distinctGlyphSilhouettes(8, 48), 4u)
        << "the owner frame must contain distinct glyph silhouettes, not one repeated fallback box";
    EXPECT_GE(distinctGlyphSilhouettes(50, 96), 4u)
        << "transformed text must preserve distinct glyph silhouettes";
    EXPECT_LE(maxChannelError, 1.0e-4f);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_glyph_text_deferred_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererFallsBackWhenGlyphPreparationEvictsAtlas)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(192, 96)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";
    renderer.setCanvasSize(192.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const QDir testSourceDirectory(
        QFileInfo(QString::fromLocal8Bit(__FILE__)).absolutePath());
    const QString bundledFontPath = testSourceDirectory.filePath(
        QStringLiteral("../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    const int bundledFontId = QFontDatabase::addApplicationFont(bundledFontPath);
    ASSERT_NE(bundledFontId, -1) << bundledFontPath.toStdString();
    const auto bundledFamilies =
        QFontDatabase::applicationFontFamilies(bundledFontId);
    ASSERT_FALSE(bundledFamilies.isEmpty());
    const QFont font(bundledFamilies.front(), 384);
    const ArtifactCore::FloatColor textColor(1.0f, 0.25f, 0.1f, 1.0f);

    // Each packet adds one distinct, supported glyph. The large raster size
    // makes the fixed 2048px atlas fill with this bounded ASCII fixture.
    QString glyphs;
    std::uint64_t paddedGlyphArea = 0;
    const QRawFont rawFont = QRawFont::fromFont(font, QFontDatabase::Any);
    for (uint codePoint = 33; codePoint <= 126; ++codePoint) {
        const auto glyph = QString::fromUcs4(&codePoint, 1);
        const auto indexes = rawFont.glyphIndexesForString(glyph);
        if (indexes.isEmpty() || indexes.front() == 0) continue;
        const QImage bitmap = rawFont.alphaMapForGlyph(
            indexes.front(), QRawFont::PixelAntialiasing);
        if (bitmap.isNull() || bitmap.width() <= 0 || bitmap.height() <= 0) continue;
        glyphs.append(glyph);
        paddedGlyphArea += static_cast<std::uint64_t>(bitmap.width() + 2) *
                           static_cast<std::uint64_t>(bitmap.height() + 2);
    }
    ASSERT_GE(glyphs.size(), 48)
        << "the bundled Roboto fixture must provide enough distinct glyphs";
    ASSERT_GT(paddedGlyphArea, 2048ull * 2048ull)
        << "the fixture's non-overlapping padded glyph area must exceed the atlas";

    // Establish and upload a clean atlas texture first. This keeps the
    // overflow assertion independent from the separate cold-atlas fallback.
    QMatrix4x4 seedTransform;
    seedTransform.scale(0.025f, 0.025f, 1.0f);
    renderer.drawTextTransformed(QRectF(0.0, 0.0, 1024.0, 600.0),
                                 glyphs.left(1), font, textColor, seedTransform);
    renderer.flushAndWait();
    renderer.clear();

    const auto drawAtlasFillingBatch = [&] {
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                   -40.0f, 2.0f, 2.0f,
                                   ArtifactCore::FloatColor(1, 1, 1, 1));
        }
        for (int index = 0; index < glyphs.size(); ++index) {
            QMatrix4x4 transform;
            transform.translate(2.0f + static_cast<float>(index % 16) * 12.0f,
                                1.0f + static_cast<float>(index / 16) * 16.0f,
                                0.0f);
            transform.scale(0.025f, 0.025f, 1.0f);
            renderer.drawTextTransformed(QRectF(0.0, 0.0, 1024.0, 600.0),
                                         glyphs.mid(index, 1), font, textColor,
                                         transform);
        }
    };

    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    drawAtlasFillingBatch();
    EXPECT_FALSE(renderer.flushDeferredPacketsAsync())
        << "full atlas replacement must reject worker recording";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore);
    const auto fallbackImage = renderer.readbackToImageF32();
    std::size_t visibleTextPixels = 0;
    for (int y = 0; y < fallbackImage.height(); ++y) {
        for (int x = 0; x < fallbackImage.width(); ++x) {
            const auto pixel = fallbackImage.getPixel(x, y);
            if (pixel.a() > 0.5f && pixel.r() > 0.5f && pixel.r() > pixel.g()) {
                ++visibleTextPixels;
            }
        }
    }
    EXPECT_GT(visibleTextPixels, 100u)
        << "owner fallback must preserve visible glyph output after atlas eviction";

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_glyph_atlas_eviction_fallback_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererFallsBackWhenPreparedGlyphSnapshotIsFull)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(192, 96)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";
    renderer.setCanvasSize(192.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const QDir testSourceDirectory(
        QFileInfo(QString::fromLocal8Bit(__FILE__)).absolutePath());
    const QString bundledFontPath = testSourceDirectory.filePath(
        QStringLiteral("../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    const int bundledFontId = QFontDatabase::addApplicationFont(bundledFontPath);
    ASSERT_NE(bundledFontId, -1) << bundledFontPath.toStdString();
    const auto bundledFamilies =
        QFontDatabase::applicationFontFamilies(bundledFontId);
    ASSERT_FALSE(bundledFamilies.isEmpty());
    const QFont font(bundledFamilies.front(), 48);
    const ArtifactCore::FloatColor textColor(1.0f, 0.25f, 0.1f, 1.0f);

    // Warm the atlas so this case isolates the fixed 4,096-entry prepared
    // sidecar bound instead of the cold-atlas creation fallback.
    QMatrix4x4 seedTransform;
    seedTransform.scale(0.25f, 0.25f, 1.0f);
    renderer.drawTextTransformed(QRectF(0.0, 0.0, 1024.0, 256.0),
                                 QStringLiteral("A"), font, textColor,
                                 seedTransform);
    renderer.flushAndWait();
    renderer.clear();

    for (int index = 0; index < 15; ++index) {
        renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                               -40.0f, 2.0f, 2.0f,
                               ArtifactCore::FloatColor(1, 1, 1, 1));
    }
    const QString maxScratchText(2048, QChar(u'A'));
    const QRawFont rawFont = QRawFont::fromFont(font, QFontDatabase::Any);
    EXPECT_EQ(rawFont.glyphIndexesForString(maxScratchText).size(),
              maxScratchText.size())
        << "the bundled fixture must retain one glyph per repeated ASCII character";
    QMatrix4x4 textTransform;
    textTransform.translate(2.0f, 2.0f, 0.0f);
    textTransform.scale(0.25f, 0.25f, 1.0f);
    const QRectF longTextRect(0.0, 0.0, 200000.0, 256.0);
    renderer.drawTextTransformed(longTextRect, maxScratchText, font,
                                 textColor, textTransform);
    renderer.drawTextTransformed(longTextRect, maxScratchText, font,
                                 textColor, textTransform);
    renderer.drawTextTransformed(longTextRect, QStringLiteral("A"), font,
                                 textColor, textTransform);

    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    EXPECT_FALSE(renderer.flushDeferredPacketsAsync())
        << "the 4,097th prepared glyph must keep this batch on the owner path";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore);
    const auto image = renderer.readbackToImageF32();
    std::size_t visibleTextPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.getPixel(x, y);
            if (pixel.a() > 0.5f && pixel.r() > 0.5f && pixel.r() > pixel.g()) {
                ++visibleTextPixels;
            }
        }
    }
    EXPECT_GT(visibleTextPixels, 100u)
        << "owner fallback must preserve visible text after snapshot capacity overflow";

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_glyph_snapshot_capacity_fallback_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     ArtifactRendererPreservesOverlayOrderAfterDeferredGlyphBatch)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(192, 96)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    ASSERT_NE(renderer.device(), nullptr);
    ASSERT_EQ(renderer.device()->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";
    renderer.setCanvasSize(192.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const QDir testSourceDirectory(
        QFileInfo(QString::fromLocal8Bit(__FILE__)).absolutePath());
    const QString bundledFontPath = testSourceDirectory.filePath(
        QStringLiteral("../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    const int bundledFontId = QFontDatabase::addApplicationFont(bundledFontPath);
    ASSERT_NE(bundledFontId, -1) << bundledFontPath.toStdString();
    const auto bundledFamilies =
        QFontDatabase::applicationFontFamilies(bundledFontId);
    ASSERT_FALSE(bundledFamilies.isEmpty());
    const QFont font(bundledFamilies.front(), 24);
    const ArtifactCore::FloatColor textColor(0.1f, 1.0f, 0.2f, 1.0f);
    const ArtifactCore::FloatColor overlayColor(0.1f, 0.2f, 0.95f, 1.0f);

    // Create a clean GPU atlas with a glyph that the worker text does not use.
    renderer.drawText(QRectF(8.0, 8.0, 48.0, 40.0), QStringLiteral("X"),
                      font, textColor);
    renderer.flushAndWait();
    renderer.clear();

    const auto drawEligibleTextBatch = [&] {
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                   -40.0f, 2.0f, 2.0f,
                                   ArtifactCore::FloatColor(1, 1, 1, 1));
        }
        renderer.drawText(QRectF(16.0, 20.0, 156.0, 42.0),
                          QStringLiteral("Deferred"), font, textColor);
    };
    const auto drawOverlay = [&] {
        renderer.drawSolidRect(20.0f, 23.0f, 24.0f, 28.0f, overlayColor);
    };

    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    drawEligibleTextBatch();
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the warm-atlas glyph batch should record on the worker";
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore);
    drawOverlay();
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore + 1);
    const auto deferredImage = renderer.readbackToImageF32();

    renderer.clear();
    drawEligibleTextBatch();
    drawOverlay();
    renderer.flushAndWait();
    const auto ownerImage = renderer.readbackToImageF32();
    ASSERT_EQ(deferredImage.width(), ownerImage.width());
    ASSERT_EQ(deferredImage.height(), ownerImage.height());
    float maxChannelError = 0.0f;
    std::size_t visibleGreenTextPixels = 0;
    for (int y = 0; y < deferredImage.height(); ++y) {
        for (int x = 0; x < deferredImage.width(); ++x) {
            const auto actual = deferredImage.getPixel(x, y);
            const auto expected = ownerImage.getPixel(x, y);
            maxChannelError = std::max({
                maxChannelError,
                std::abs(actual.r() - expected.r()),
                std::abs(actual.g() - expected.g()),
                std::abs(actual.b() - expected.b()),
                std::abs(actual.a() - expected.a())});
            if (actual.a() > 0.5f && actual.g() > 0.5f &&
                actual.g() > actual.r() && (x < 20 || x >= 44 || y < 23 || y >= 51)) {
                ++visibleGreenTextPixels;
            }
        }
    }
    EXPECT_LE(maxChannelError, 1.0e-4f);
    EXPECT_GT(visibleGreenTextPixels, 100u)
        << "glyph pixels outside the later overlay must remain visible";
    const auto overlayPixel = deferredImage.getPixel(30, 36);
    EXPECT_GT(overlayPixel.b(), 0.8f);
    EXPECT_LT(overlayPixel.r(), 0.4f);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_deferred_glyph_overlay_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     SubmittedPlacementRangeFallsBackAfterQueueDrain)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128, true)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    if (!renderer.clonerGridPlacementGpuReady()) {
        GTEST_SKIP() << "Diligent backend could not create the Cloner GPU pipelines";
    }

    auto device = renderer.device();
    ASSERT_NE(device, nullptr);
    constexpr std::array<std::uint8_t, 4> texel{255, 255, 255, 255};
    Diligent::TextureDesc textureDesc;
    textureDesc.Name = "Cloner stale range fallback source";
    textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    textureDesc.Usage = Diligent::USAGE_IMMUTABLE;
    textureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData subresource;
    subresource.pData = texel.data();
    subresource.Stride = texel.size();
    Diligent::TextureData initialData;
    initialData.pSubResources = &subresource;
    initialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> sourceTexture;
    device->CreateTexture(textureDesc, &initialData, &sourceTexture);
    ASSERT_NE(sourceTexture, nullptr);
    auto* sourceView = sourceTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(sourceView, nullptr);

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    Artifact::ClonerGridPlacementRange placementRange;
    ASSERT_TRUE(renderer.dispatchClonerGridPlacement(
        2, 1, 1, 40.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.75f,
        placementRange));

    // Submitting the dispatch ends the range's batch lifetime. The stale GPU
    // range must be rejected so the caller can safely use its CPU transform.
    renderer.flushAndWait();
    QMatrix4x4 identity;
    identity.setToIdentity();
    for (std::uint32_t index = 0; index < placementRange.count; ++index) {
        EXPECT_FALSE(renderer.drawSpriteTransformedClonerGrid(
            64.0f, 64.0f, 12.0f, 8.0f, identity, identity, sourceView,
            1.0f, placementRange,
            placementRange.firstIndex + index)) << "placement=" << index;
    }

    renderer.drawSpriteTransformed(47.0f, 63.0f, 12.0f, 8.0f, identity,
                                   sourceView, 1.0f);
    renderer.drawSpriteTransformed(64.0f, 63.0f, 12.0f, 8.0f, identity,
                                   sourceView, 1.0f);
    renderer.drawSpriteTransformed(87.0f, 63.0f, 12.0f, 8.0f, identity,
                                   sourceView, 1.0f);
    renderer.flushAndWait();

    const auto image = renderer.readbackToImageF32();
    const auto components = findVisibleComponents(image);
    ASSERT_EQ(components.size(), 3u);
    constexpr std::array<float, 3> expectedCenterX{53.0f, 70.0f, 93.0f};
    for (std::size_t index = 0; index < components.size(); ++index) {
        const auto& component = components[index];
        EXPECT_NEAR((component.left + component.right) * 0.5f,
                    expectedCenterX[index], 1.5f);
        EXPECT_NEAR((component.top + component.bottom) * 0.5f, 67.0f, 1.5f);
    }

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_cloner_stale_range_fallback_%1.png")
            .arg(backend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}

TEST(ClonerGpuRenderContractTest,
     DeferredClonerRangeFallsBackAcrossImmediateClearBarrier)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 128, true)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    if (!renderer.clonerGridPlacementGpuReady()) {
        GTEST_SKIP() << "Diligent backend could not create the Cloner GPU pipelines";
    }

    const auto requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("d3d12")).toLower();
    const auto expectedDeviceType = requestedBackend == QStringLiteral("vulkan")
        ? Diligent::RENDER_DEVICE_TYPE_VULKAN
        : Diligent::RENDER_DEVICE_TYPE_D3D12;
    auto device = renderer.device();
    ASSERT_NE(device, nullptr);
    ASSERT_EQ(device->GetDeviceInfo().Type, expectedDeviceType)
        << "headless initialization must not silently test a fallback backend";

    constexpr std::array<std::uint8_t, 4> texel{255, 255, 255, 255};
    Diligent::TextureDesc textureDesc;
    textureDesc.Name = "Cloner immediate clear barrier fallback source";
    textureDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    textureDesc.Usage = Diligent::USAGE_IMMUTABLE;
    textureDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData subresource;
    subresource.pData = texel.data();
    subresource.Stride = texel.size();
    Diligent::TextureData initialData;
    initialData.pSubResources = &subresource;
    initialData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> sourceTexture;
    device->CreateTexture(textureDesc, &initialData, &sourceTexture);
    ASSERT_NE(sourceTexture, nullptr);
    auto* sourceView = sourceTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(sourceView, nullptr);

    const ArtifactCore::FloatColor transparent(0.0f, 0.0f, 0.0f, 0.0f);
    renderer.clearRenderTarget(transparent);
    const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
    for (int index = 0; index < 15; ++index) {
        renderer.drawSolidRect(-48.0f - static_cast<float>(index) * 3.0f,
                               -48.0f, 2.0f, 2.0f, invisible);
    }

    Artifact::ClonerGridPlacementRange placementRange;
    ASSERT_TRUE(renderer.dispatchClonerGridPlacement(
        2, 1, 1, 40.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.75f,
        placementRange));
    ASSERT_EQ(placementRange.count, 3u);
    const auto workerSubmitsBeforeHandoff =
        renderer.deferredWorkerSubmitCount();
    ASSERT_TRUE(renderer.flushDeferredPacketsAsync())
        << "the 15 filler packets plus Cloner dispatch should use deferred recording";

    // This immediate clear is ordered after the worker command list. The
    // dispatch range belongs to the submitted batch, so the later sprites
    // must reject it and use their CPU transforms instead.
    renderer.clearRenderTarget(transparent);
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(),
              workerSubmitsBeforeHandoff + 1);

    QMatrix4x4 identity;
    identity.setToIdentity();
    for (std::uint32_t index = 0; index < placementRange.count; ++index) {
        EXPECT_FALSE(renderer.drawSpriteTransformedClonerGrid(
            64.0f, 64.0f, 12.0f, 8.0f, identity, identity, sourceView,
            1.0f, placementRange,
            placementRange.firstIndex + index)) << "placement=" << index;
    }
    renderer.drawSpriteTransformed(47.0f, 63.0f, 12.0f, 8.0f, identity,
                                   sourceView, 1.0f);
    renderer.drawSpriteTransformed(64.0f, 63.0f, 12.0f, 8.0f, identity,
                                   sourceView, 1.0f);
    renderer.drawSpriteTransformed(87.0f, 63.0f, 12.0f, 8.0f, identity,
                                   sourceView, 1.0f);
    renderer.flushAndWait();

    const auto image = renderer.readbackToImageF32();
    const auto components = findVisibleComponents(image);
    ASSERT_EQ(components.size(), 3u);
    constexpr std::array<float, 3> expectedCenterX{53.0f, 70.0f, 93.0f};
    for (std::size_t index = 0; index < components.size(); ++index) {
        const auto& component = components[index];
        EXPECT_NEAR((component.left + component.right) * 0.5f,
                    expectedCenterX[index], 1.5f);
        EXPECT_NEAR((component.top + component.bottom) * 0.5f, 67.0f, 1.5f);
    }

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_cloner_clear_barrier_fallback_%1.png")
            .arg(requestedBackend));
    ASSERT_TRUE(renderer.readbackToImage().save(capturePath, "PNG"))
        << capturePath.toStdString();
}
