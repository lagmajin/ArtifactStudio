#include <gtest/gtest.h>

#include <Texture.h>
#include <TextureView.h>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include <future>
#include <thread>

#include "DiligentHeadlessRenderer.hpp"
import Color.Float;
import Graphics.ParticleData;
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
     ArtifactRendererKeepsGlyphTextOnOwnerWhenDeferredBatchIsEligible)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(128, 96)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }
    renderer.setCanvasSize(128.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    const ArtifactCore::FloatColor invisible(1.0f, 1.0f, 1.0f, 1.0f);
    const auto drawEligibleBatchWithText = [&] {
        for (int index = 0; index < 15; ++index) {
            renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                                   -40.0f, 2.0f, 2.0f, invisible);
        }
        renderer.drawText(QRectF(8.0, 8.0, 112.0, 40.0), QStringLiteral("Deferred"),
                          QFont(QStringLiteral("Arial"), 24),
                          ArtifactCore::FloatColor(0.1f, 1.0f, 0.2f, 1.0f));
    };

    const quint64 workerSubmitCountBefore = renderer.deferredWorkerSubmitCount();
    renderer.clear();
    drawEligibleBatchWithText();
    renderer.flushAndWait();
    const auto immediateImage = renderer.readbackToImageF32();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore);

    renderer.clear();
    drawEligibleBatchWithText();
    EXPECT_FALSE(renderer.flushDeferredPacketsAsync())
        << "glyph shaping and atlas mutation must stay on the owner submit path";
    renderer.flushAndWait();
    EXPECT_EQ(renderer.deferredWorkerSubmitCount(), workerSubmitCountBefore);

    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), immediateImage.width());
    ASSERT_EQ(image.height(), immediateImage.height());
    std::size_t greenTextPixels = 0;
    float maxChannelError = 0.0f;
    for (int y = 8; y < 48; ++y) {
        for (int x = 8; x < 120; ++x) {
            const auto pixel = image.getPixel(x, y);
            if (pixel.a() > 0.5f && pixel.g() > 0.5f && pixel.g() > pixel.r()) {
                ++greenTextPixels;
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
    EXPECT_LE(maxChannelError, 1.0e-4f);

    const auto captureDirectory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("cloner_gpu_render_captures"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto backend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND", QStringLiteral("unknown"));
    const auto capturePath = QDir(captureDirectory).filePath(
        QStringLiteral("artifact_renderer_glyph_text_owner_fallback_%1.png")
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
}
