#include <gtest/gtest.h>

#include <Texture.h>
#include <TextureView.h>
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <future>
#include <thread>

#include "DiligentHeadlessRenderer.hpp"
import Color.Float;
import Image.ImageF32x4_RGBA;
import Artifact.Render.DiligentImmediateSubmitter;
import Artifact.Render.ShaderManager;
import Artifact.Render.Config;

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
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const bool immediateCovered = immediate.getPixel(x, y).a() > 0.5f;
            const bool deferredCovered = deferred.getPixel(x, y).a() > 0.5f;
            coverageMismatches += immediateCovered != deferredCovered ? 1u : 0u;
        }
    }
    EXPECT_EQ(coverageMismatches, 0u);
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
        return renderer.readbackToImageF32();
    };

    const auto immediate = renderTwoViewports(false);
    const auto deferred = renderTwoViewports(true);
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
    Artifact::ShaderManager workerShaders(device, Artifact::RenderConfig::MainRTVFormat);
    ASSERT_TRUE(workerShaders.isInitialized());
    workerShaders.createShaders();
    workerShaders.createPSOs();
    Artifact::DiligentImmediateSubmitter submitter;
    submitter.createBuffers(device, Artifact::RenderConfig::MainRTVFormat);
    submitter.setPSOs(shaders);
    Artifact::DiligentImmediateSubmitter workerSubmitter;
    const auto makeBuffer = [&] {
        Artifact::RenderCommandBuffer buffer;
        buffer.reserve(16);
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
        workerSubmitter.createBuffers(device, Artifact::RenderConfig::MainRTVFormat);
        workerSubmitter.setPSOs(workerShaders);
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
    EXPECT_EQ(buffer.packets().size(), 16u);
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
}
