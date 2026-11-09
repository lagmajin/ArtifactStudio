#include <BasicMath.hpp>
#include <Buffer.h>
#include <DeviceContext.h>
#include <EngineFactoryVk.h>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QTemporaryDir>
#include <QVector3D>
#include <RefCntAutoPtr.hpp>
#include <Texture.h>
#include <TopLevelAS.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>
import Graphics.RayTracingManager;
import Graphics.GPUcomputeContext;
import Graphics.MeshRenderer;
import Core.Light;
import Image.OpenEXR;
import IO.ImageImporter;
import Mesh;
import Utils.String.UniString;
namespace {
using namespace Diligent;
constexpr Uint32 kRayTraceTestLogicalSize = 32;
constexpr Uint32 kRayTraceTestImageSize = 256;
constexpr Uint32 kRayTraceTestPixelScale = kRayTraceTestImageSize / kRayTraceTestLogicalSize;
class RayTracingAcceptanceTest : public testing::Test {
  protected:
    void SetUp() override {
        auto* factory = LoadAndGetEngineFactoryVk();
        if (!factory) {
            GTEST_SKIP() << "Diligent Vulkan backend is unavailable";
        }
        EngineVkCreateInfo createInfo{};
        createInfo.Features.MultithreadedResourceCreation = DEVICE_FEATURE_STATE_DISABLED;
        createInfo.Features.RayTracing = DEVICE_FEATURE_STATE_ENABLED;
        factory->CreateDeviceAndContextsVk(createInfo, &device_, &context_);
        if (!device_ || !context_) {
            GTEST_SKIP() << "Could not create a headless Vulkan device";
        }
        manager_ = ArtifactCore::createRayTracingManager();
        ASSERT_TRUE(manager_->initialize(device_.RawPtr()));
        if (!manager_->isSupported()) {
            GTEST_SKIP() << "The active Vulkan adapter does not support ray tracing";
        }
    }
    void TearDown() override {
        if (context_) {
            context_->Flush();
            context_->WaitForIdle();
        }
        manager_.reset();
        context_.Release();
        device_.Release();
    }
    RefCntAutoPtr<IBuffer> makeBuffer(const void* data, Uint64 size, Uint32 stride,
                                      BIND_FLAGS bindFlags,
                                      BUFFER_MODE mode = BUFFER_MODE_UNDEFINED) {
        BufferDesc desc;
        desc.Name = "RayTracingAcceptanceGeometry";
        desc.Size = size;
        desc.Usage = USAGE_DEFAULT;
        desc.BindFlags = bindFlags | BIND_RAY_TRACING;
        desc.ElementByteStride = stride;
        desc.Mode = mode;
        BufferData initialData{data, size, context_.RawPtr()};
        RefCntAutoPtr<IBuffer> buffer;
        device_->CreateBuffer(desc, &initialData, &buffer);
        return buffer;
    }
    void addGeometry(const char* id, IBuffer* vertices, Uint32 vertexCount, IBuffer* indices,
                     Uint32 indexCount, const float4x4& transform = float4x4::Identity(),
                     Uint64 geometryRevision = 1, Uint64 topologyRevision = 1,
                     const float* shadingVertices = nullptr,
                     const Uint32* shadingIndices = nullptr,
                     const float* shadingNormals = nullptr) {
        ArtifactCore::RTGeometryData geometry;
        geometry.pVertexBuffer = vertices;
        geometry.vertexCount = vertexCount;
        geometry.pIndexBuffer = indices;
        geometry.indexCount = indexCount;
        geometry.transform = transform;
        geometry.geometryRevision = geometryRevision;
        geometry.topologyRevision = topologyRevision;
        ASSERT_TRUE(manager_->createOrUpdateBLAS(ArtifactCore::UniString(id), geometry));
        if (shadingVertices && shadingIndices) {
            ASSERT_TRUE(manager_->updateShadingGeometry(ArtifactCore::UniString(id),
                                                        shadingVertices, vertexCount,
                                                        shadingIndices, indexCount,
                                                        shadingNormals));
        }
    }
    bool updateBuffer(IBuffer* buffer, const void* data, Uint64 size) {
        if (!buffer || !context_) return false;
        context_->UpdateBuffer(buffer, 0, size, data, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        return true;
    }
    std::array<float, 4> pixelAt(Uint32 x, Uint32 y) {
        constexpr Uint32 width = kRayTraceTestImageSize;
        constexpr Uint32 height = kRayTraceTestImageSize;
        auto* outputView = manager_->traceOutputSRV();
        EXPECT_NE(outputView, nullptr);
        if (!outputView) return {};
        auto* output = outputView->GetTexture();
        EXPECT_NE(output, nullptr);
        if (!output) return {};
        TextureDesc stagingDesc;
        stagingDesc.Name = "RayTracingAcceptanceReadback";
        stagingDesc.Type = RESOURCE_DIM_TEX_2D;
        stagingDesc.Width = width;
        stagingDesc.Height = height;
        stagingDesc.MipLevels = 1;
        stagingDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
        stagingDesc.Usage = USAGE_STAGING;
        stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
        RefCntAutoPtr<ITexture> staging;
        device_->CreateTexture(stagingDesc, nullptr, &staging);
        EXPECT_NE(staging, nullptr);
        if (!staging) return {};
        CopyTextureAttribs copy(output, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->CopyTexture(copy);
        context_->Flush();
        context_->WaitForIdle();
        MappedTextureSubresource mapped{};
        context_->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
        EXPECT_NE(mapped.pData, nullptr);
        if (!mapped.pData) return {};
        const Uint32 sampleX = x * kRayTraceTestPixelScale + kRayTraceTestPixelScale / 2;
        const Uint32 sampleY = y * kRayTraceTestPixelScale + kRayTraceTestPixelScale / 2;
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData) + sampleY * mapped.Stride;
        std::array<float, 4> pixel{};
        std::memcpy(pixel.data(), row + sampleX * sizeof(pixel), sizeof(pixel));
        context_->UnmapTextureSubresource(staging, 0, 0);
        return pixel;
    }
    float maximumGreenDifference(const std::array<float, 4>* reference) {
        float maximumDifference = 0.0f;
        for (Uint32 y = 0; y < kRayTraceTestLogicalSize; ++y) {
            for (Uint32 x = 0; x < kRayTraceTestLogicalSize; ++x) {
                const auto current = pixelAt(x, y);
                const auto referencePixel = reference[y * kRayTraceTestLogicalSize + x];
                maximumDifference = std::max(
                    maximumDifference, std::fabs(current[1] - referencePixel[1]));
            }
        }
        return maximumDifference;
    }
    std::vector<std::array<float, 4>> readOutputPixels() {
        auto* outputView = manager_->traceOutputSRV();
        EXPECT_NE(outputView, nullptr);
        if (!outputView) return {};
        auto* outputTexture = outputView->GetTexture();
        EXPECT_NE(outputTexture, nullptr);
        if (!outputTexture) return {};
        const auto& outputDesc = outputTexture->GetDesc();
        TextureDesc stagingDesc;
        stagingDesc.Name = "RayTracingAcceptanceFullReadback";
        stagingDesc.Type = RESOURCE_DIM_TEX_2D;
        stagingDesc.Width = outputDesc.Width;
        stagingDesc.Height = outputDesc.Height;
        stagingDesc.MipLevels = 1;
        stagingDesc.Format = outputDesc.Format;
        stagingDesc.Usage = USAGE_STAGING;
        stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
        RefCntAutoPtr<ITexture> staging;
        device_->CreateTexture(stagingDesc, nullptr, &staging);
        EXPECT_NE(staging, nullptr);
        if (!staging) return {};
        CopyTextureAttribs copy(outputTexture,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                staging,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->CopyTexture(copy);
        context_->Flush();
        context_->WaitForIdle();
        MappedTextureSubresource mapped{};
        context_->MapTextureSubresource(staging, 0, 0, MAP_READ,
                                        MAP_FLAG_NONE, nullptr, mapped);
        EXPECT_NE(mapped.pData, nullptr);
        if (!mapped.pData) return {};
        std::vector<std::array<float, 4>> pixels(
            static_cast<size_t>(outputDesc.Width) * outputDesc.Height);
        const auto* mappedBytes =
            static_cast<const std::uint8_t*>(mapped.pData);
        for (Uint32 y = 0; y < outputDesc.Height; ++y) {
            const auto* row = mappedBytes + y * mapped.Stride;
            std::memcpy(pixels.data() +
                            static_cast<size_t>(y) * outputDesc.Width,
                        row, static_cast<size_t>(outputDesc.Width) *
                                 sizeof(std::array<float, 4>));
        }
        context_->UnmapTextureSubresource(staging, 0, 0);
        return pixels;
    }
    float maximumGreenDifference(
        const std::vector<std::array<float, 4>>& reference) {
        const auto current = readOutputPixels();
        EXPECT_EQ(current.size(), reference.size());
        const size_t count = std::min(current.size(), reference.size());
        float maximumDifference = 0.0f;
        for (size_t pixel = 0; pixel < count; ++pixel) {
            maximumDifference = std::max(
                maximumDifference,
                std::fabs(current[pixel][1] - reference[pixel][1]));
        }
        return maximumDifference;
    }
    bool saveOutputFrame(const char* fileName) {
        constexpr Uint32 width = kRayTraceTestImageSize;
        constexpr Uint32 height = kRayTraceTestImageSize;
        auto* outputView = manager_->traceOutputSRV();
        EXPECT_NE(outputView, nullptr);
        if (!outputView) return false;
        auto* output = outputView->GetTexture();
        EXPECT_NE(output, nullptr);
        if (!output) return false;
        TextureDesc stagingDesc;
        stagingDesc.Name = "RayTracingFrameExport";
        stagingDesc.Type = RESOURCE_DIM_TEX_2D;
        stagingDesc.Width = width;
        stagingDesc.Height = height;
        stagingDesc.MipLevels = 1;
        stagingDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
        stagingDesc.Usage = USAGE_STAGING;
        stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
        RefCntAutoPtr<ITexture> staging;
        device_->CreateTexture(stagingDesc, nullptr, &staging);
        EXPECT_NE(staging, nullptr);
        if (!staging) return false;
        CopyTextureAttribs copy(output, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->CopyTexture(copy);
        context_->Flush();
        context_->WaitForIdle();
        MappedTextureSubresource mapped{};
        context_->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
        EXPECT_NE(mapped.pData, nullptr);
        if (!mapped.pData) return false;
        QImage image(width, height, QImage::Format_RGBA8888);
        for (Uint32 y = 0; y < height; ++y) {
            const auto* sourceRow =
                static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.Stride;
            auto* destinationRow = image.scanLine(static_cast<int>(y));
            for (Uint32 x = 0; x < width; ++x) {
                const auto* source =
                    reinterpret_cast<const float*>(sourceRow + x * sizeof(float) * 4);
                auto* destination = destinationRow + x * 4;
                for (Uint32 channel = 0; channel < 4; ++channel) {
                    float value = std::clamp(source[channel], 0.0f, 1.0f);
                    if (channel < 3) {
                        value = value <= 0.0031308f
                                    ? 12.92f * value
                                    : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
                    }
                    destination[channel] = static_cast<std::uint8_t>(std::lround(value * 255.0f));
                }
            }
        }
        context_->UnmapTextureSubresource(staging, 0, 0);
        const char* configuredOutputDir = std::getenv("ARTIFACT_RT_FRAME_OUTPUT_DIR");
        const auto outputDir = configuredOutputDir && configuredOutputDir[0] != '\0'
                                   ? std::filesystem::path(configuredOutputDir)
                                   : std::filesystem::current_path() / "ray-tracing-frames";
        std::filesystem::create_directories(outputDir);
        const auto outputPath = outputDir / fileName;
        EXPECT_TRUE(image.save(QString::fromStdString(outputPath.string()), "PNG"))
            << "Could not save ray tracing frame: " << outputPath.string();
        return true;
    }
    std::array<float, 4> expectHit(Uint32 x, Uint32 y = 16) {
        const auto pixel = pixelAt(x, y);
        EXPECT_GT(pixel[0], 0.02f);
        EXPECT_GT(pixel[0], pixel[1]);
        EXPECT_GT(pixel[0], pixel[2]);
        EXPECT_NEAR(pixel[3], 1.0f, 0.03f);
        return pixel;
    }
    void expectMiss(Uint32 x, Uint32 y = 16) {
        const auto pixel = pixelAt(x, y);
        EXPECT_GT(pixel[2], pixel[1]) << "miss pixel (" << x << ", " << y << ")";
        EXPECT_GT(pixel[1], pixel[0]) << "miss pixel (" << x << ", " << y << ")";
        EXPECT_NEAR(pixel[3], 1.0f, 0.03f);
    }
    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IDeviceContext> context_;
    std::unique_ptr<ArtifactCore::IRayTracingManager> manager_;
};

TEST_F(RayTracingAcceptanceTest, AppliesClearcoatToDirectLighting) {
    constexpr std::array<float, 9> triangleVertices{
        -0.8f, -0.8f, 0.0f, 0.8f, -0.8f, 0.0f, 0.0f, 0.8f, 0.0f};
    constexpr std::array<std::uint32_t, 3> triangleIndices{0, 1, 2};
    auto vertexBuffer = makeBuffer(
        triangleVertices.data(), sizeof(triangleVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        triangleIndices.data(), sizeof(triangleIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("clearcoat-isolation", vertexBuffer.RawPtr(), 3,
                indexBuffer.RawPtr(), 3, float4x4::Identity(), 1, 1,
                triangleVertices.data(), triangleIndices.data());

    ArtifactCore::RTPbrMaterial baseMaterial;
    baseMaterial.baseColor[0] = 0.65f;
    baseMaterial.baseColor[1] = 0.65f;
    baseMaterial.baseColor[2] = 0.65f;
    baseMaterial.roughness = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-isolation"), baseMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto basePixel = pixelAt(16, 16);

    auto coatedMaterial = baseMaterial;
    coatedMaterial.clearcoat = 1.0f;
    coatedMaterial.clearcoatRoughness = 0.2f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("18-clearcoat-direct-light.png"));
    const auto coatedPixel = pixelAt(16, 16);
    const float maximumChannelDifference = std::max(
        std::fabs(coatedPixel[0] - basePixel[0]),
        std::max(std::fabs(coatedPixel[1] - basePixel[1]),
                 std::fabs(coatedPixel[2] - basePixel[2])));
    EXPECT_GT(maximumChannelDifference, 0.001f);

    coatedMaterial.clearcoatRoughness = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughCoatedPixel = pixelAt(16, 16);
    const float clearcoatRoughnessDifference = std::max(
        std::fabs(roughCoatedPixel[0] - coatedPixel[0]),
        std::max(std::fabs(roughCoatedPixel[1] - coatedPixel[1]),
                 std::fabs(roughCoatedPixel[2] - coatedPixel[2])));
    EXPECT_GT(clearcoatRoughnessDifference, 0.001f)
        << "clearcoat roughness should change the direct-light reflection response";
    coatedMaterial.clearcoatRoughness = 0.2f;

    QTemporaryDir coatNormalDirectory;
    ASSERT_TRUE(coatNormalDirectory.isValid());
    const QString coatNormalPath = coatNormalDirectory.filePath(
        QStringLiteral("coat-normal.png"));
    QImage coatNormalImage(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(coatNormalImage.isNull());
    coatNormalImage.fill(QColor(204, 128, 230, 255));
    ASSERT_TRUE(coatNormalImage.save(coatNormalPath, "PNG"));
    coatedMaterial.clearcoatNormalTexture =
        ArtifactCore::UniString(coatNormalPath);
    coatedMaterial.clearcoatNormalStrength = 1.0f;
    coatedMaterial.clearcoatNormalUvRotationDegrees = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unrotatedCoatNormalPixel = pixelAt(16, 16);

    coatedMaterial.clearcoatNormalUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedCoatNormalPixel = pixelAt(16, 16);
    const float coatNormalRotationDifference = std::max(
        std::fabs(unrotatedCoatNormalPixel[0] - rotatedCoatNormalPixel[0]),
        std::max(std::fabs(unrotatedCoatNormalPixel[1] - rotatedCoatNormalPixel[1]),
                 std::fabs(unrotatedCoatNormalPixel[2] - rotatedCoatNormalPixel[2])));
    EXPECT_GT(coatNormalRotationDifference, 0.001f)
        << "clearcoat normal UV rotation should change the coat reflection "
           "direction";

    const auto captureClearcoatRoughness = [this, &coatedMaterial](
                                               float clearcoatRoughness) {
        coatedMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
        coatedMaterial.clearcoatRoughness = clearcoatRoughness;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto minimumClearcoatRoughnessPixel =
        captureClearcoatRoughness(0.0f);
    const auto belowMinimumClearcoatRoughnessPixel =
        captureClearcoatRoughness(-0.5f);
    const auto maximumClearcoatRoughnessPixel =
        captureClearcoatRoughness(1.0f);
    const auto aboveMaximumClearcoatRoughnessPixel =
        captureClearcoatRoughness(1.5f);
    const float clearcoatRoughnessResponse = std::max(
        std::fabs(maximumClearcoatRoughnessPixel[0] -
                  minimumClearcoatRoughnessPixel[0]),
        std::max(std::fabs(maximumClearcoatRoughnessPixel[1] -
                           minimumClearcoatRoughnessPixel[1]),
                 std::fabs(maximumClearcoatRoughnessPixel[2] -
                           minimumClearcoatRoughnessPixel[2])));
    EXPECT_GT(clearcoatRoughnessResponse, 0.001f)
        << "the fixture should distinguish minimum and maximum clearcoat roughness";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumClearcoatRoughnessPixel[channel],
                    minimumClearcoatRoughnessPixel[channel], 1.0e-5f)
            << "clearcoat roughness below zero should clamp to zero";
        EXPECT_NEAR(aboveMaximumClearcoatRoughnessPixel[channel],
                    maximumClearcoatRoughnessPixel[channel], 1.0e-5f)
            << "clearcoat roughness above one should clamp to one";
    }

    auto captureClearcoatIor = [this, &coatedMaterial](float clearcoatIOR) {
        coatedMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
        coatedMaterial.clearcoatNormalStrength = 1.0f;
        coatedMaterial.clearcoatIOR = clearcoatIOR;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto minimumClearcoatIorPixel = captureClearcoatIor(1.0f);
    const auto belowMinimumClearcoatIorPixel = captureClearcoatIor(0.5f);
    const auto maximumClearcoatIorPixel = captureClearcoatIor(2.0f);
    const auto aboveMaximumClearcoatIorPixel = captureClearcoatIor(3.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumClearcoatIorPixel[channel],
                    minimumClearcoatIorPixel[channel], 1.0e-5f)
            << "clearcoat IOR below 1.0 should clamp to the lower bound";
        EXPECT_NEAR(aboveMaximumClearcoatIorPixel[channel],
                    maximumClearcoatIorPixel[channel], 1.0e-5f)
            << "clearcoat IOR above 2.0 should clamp to the upper bound";
    }

    const auto captureClearcoatTint = [this, &coatedMaterial](
                                          std::size_t tintChannel, float tint) {
        coatedMaterial.clearcoat = 1.0f;
        coatedMaterial.sheen = 0.0f;
        coatedMaterial.clearcoatTint[0] = 1.0f;
        coatedMaterial.clearcoatTint[1] = 1.0f;
        coatedMaterial.clearcoatTint[2] = 1.0f;
        coatedMaterial.clearcoatTint[tintChannel] = tint;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    for (std::size_t tintChannel = 0; tintChannel < 3; ++tintChannel) {
        const auto minimumClearcoatTintPixel =
            captureClearcoatTint(tintChannel, 0.0f);
        const auto belowMinimumClearcoatTintPixel =
            captureClearcoatTint(tintChannel, -0.5f);
        const auto maximumClearcoatTintPixel =
            captureClearcoatTint(tintChannel, 1.0f);
        const auto aboveMaximumClearcoatTintPixel =
            captureClearcoatTint(tintChannel, 1.5f);
        EXPECT_GT(std::fabs(maximumClearcoatTintPixel[tintChannel] -
                            minimumClearcoatTintPixel[tintChannel]),
                  0.001f)
            << "clearcoat tint should affect its matching output channel "
            << tintChannel;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            if (channel != tintChannel) {
                EXPECT_NEAR(maximumClearcoatTintPixel[channel],
                            minimumClearcoatTintPixel[channel], 1.0e-5f)
                    << "clearcoat tint channel " << tintChannel
                    << " should not alter output channel " << channel;
            }
            EXPECT_NEAR(belowMinimumClearcoatTintPixel[channel],
                        minimumClearcoatTintPixel[channel], 1.0e-5f)
                << "clearcoat tint below zero should clamp to zero, tint channel "
                << tintChannel << ", output channel " << channel;
            EXPECT_NEAR(aboveMaximumClearcoatTintPixel[channel],
                        maximumClearcoatTintPixel[channel], 1.0e-5f)
                << "clearcoat tint above one should clamp to one, tint channel "
                << tintChannel << ", output channel " << channel;
        }
    }

    const auto captureSheenRoughness = [this, &coatedMaterial](
                                          float sheenRoughness) {
        coatedMaterial.clearcoat = 0.0f;
        coatedMaterial.sheen = 1.0f;
        coatedMaterial.sheenRoughness = sheenRoughness;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto minimumSheenRoughnessPixel = captureSheenRoughness(0.0f);
    const auto belowMinimumSheenRoughnessPixel = captureSheenRoughness(-0.5f);
    const auto maximumSheenRoughnessPixel = captureSheenRoughness(1.0f);
    const auto aboveMaximumSheenRoughnessPixel = captureSheenRoughness(1.5f);
    const float sheenRoughnessResponse = std::max(
        std::fabs(maximumSheenRoughnessPixel[0] -
                  minimumSheenRoughnessPixel[0]),
        std::max(std::fabs(maximumSheenRoughnessPixel[1] -
                           minimumSheenRoughnessPixel[1]),
                 std::fabs(maximumSheenRoughnessPixel[2] -
                           minimumSheenRoughnessPixel[2])));
    EXPECT_GT(sheenRoughnessResponse, 0.001f)
        << "the fixture should distinguish minimum and maximum sheen roughness";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumSheenRoughnessPixel[channel],
                    minimumSheenRoughnessPixel[channel], 1.0e-5f)
            << "sheen roughness below zero should clamp to zero";
        EXPECT_NEAR(aboveMaximumSheenRoughnessPixel[channel],
                    maximumSheenRoughnessPixel[channel], 1.0e-5f)
            << "sheen roughness above one should clamp to one";
    }

    const auto captureSheenTint = [this, &coatedMaterial](
                                      std::size_t tintChannel, float tint) {
        coatedMaterial.sheenTint[0] = 1.0f;
        coatedMaterial.sheenTint[1] = 1.0f;
        coatedMaterial.sheenTint[2] = 1.0f;
        coatedMaterial.sheenTint[tintChannel] = tint;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    for (std::size_t tintChannel = 0; tintChannel < 3; ++tintChannel) {
        const auto minimumSheenTintPixel = captureSheenTint(tintChannel, 0.0f);
        const auto belowMinimumSheenTintPixel =
            captureSheenTint(tintChannel, -0.5f);
        const auto maximumSheenTintPixel = captureSheenTint(tintChannel, 1.0f);
        const auto aboveMaximumSheenTintPixel =
            captureSheenTint(tintChannel, 1.5f);
        EXPECT_GT(std::fabs(maximumSheenTintPixel[tintChannel] -
                            minimumSheenTintPixel[tintChannel]),
                  0.001f)
            << "sheen tint should affect its matching output channel "
            << tintChannel;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            if (channel != tintChannel) {
                EXPECT_NEAR(maximumSheenTintPixel[channel],
                            minimumSheenTintPixel[channel], 1.0e-5f)
                    << "sheen tint channel " << tintChannel
                    << " should not alter output channel " << channel;
            }
            EXPECT_NEAR(belowMinimumSheenTintPixel[channel],
                        minimumSheenTintPixel[channel], 1.0e-5f)
                << "sheen tint below zero should clamp to zero, tint channel "
                << tintChannel << ", output channel " << channel;
            EXPECT_NEAR(aboveMaximumSheenTintPixel[channel],
                        maximumSheenTintPixel[channel], 1.0e-5f)
                << "sheen tint above one should clamp to one, tint channel "
                << tintChannel << ", output channel " << channel;
        }
    }

    const auto captureSpecular = [&](float specular) {
        coatedMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
        coatedMaterial.clearcoat = 0.0f;
        coatedMaterial.sheen = 0.0f;
        coatedMaterial.metallic = 0.0f;
        coatedMaterial.ior = 1.5f;
        coatedMaterial.specular = specular;
        coatedMaterial.specularTint[0] = coatedMaterial.specularTint[1] =
            coatedMaterial.specularTint[2] = 1.0f;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto minimumSpecular = captureSpecular(0.0f);
    const auto belowMinimumSpecular = captureSpecular(-0.5f);
    const auto defaultSpecular = captureSpecular(0.5f);
    const auto nonFiniteSpecular = captureSpecular(
        std::numeric_limits<float>::quiet_NaN());
    const auto maximumSpecular = captureSpecular(1.0f);
    const auto aboveMaximumSpecular = captureSpecular(1.5f);
    float specularResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumSpecular[channel], minimumSpecular[channel],
                    1.0e-5f)
            << "specular below zero should clamp to zero, channel " << channel;
        EXPECT_NEAR(nonFiniteSpecular[channel], defaultSpecular[channel],
                    1.0e-5f)
            << "non-finite specular should use its 0.5 fallback, channel "
            << channel;
        EXPECT_NEAR(aboveMaximumSpecular[channel], maximumSpecular[channel],
                    1.0e-5f)
            << "specular above one should clamp to one, channel " << channel;
        specularResponse = std::max(
            specularResponse,
            std::fabs(maximumSpecular[channel] - minimumSpecular[channel]));
    }
    EXPECT_GT(specularResponse, 0.001f)
        << "the specular factor should measurably scale dielectric reflection";

    const auto captureLobeAmounts = [&](float clearcoat, float sheen) {
        coatedMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
        coatedMaterial.clearcoatRoughness = 0.2f;
        coatedMaterial.clearcoatIOR = 1.5f;
        coatedMaterial.clearcoat = clearcoat;
        coatedMaterial.sheenRoughness = 0.25f;
        coatedMaterial.sheen = sheen;
        coatedMaterial.clearcoatTint[0] = coatedMaterial.clearcoatTint[1] =
            coatedMaterial.clearcoatTint[2] = 1.0f;
        coatedMaterial.sheenTint[0] = coatedMaterial.sheenTint[1] =
            coatedMaterial.sheenTint[2] = 1.0f;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("clearcoat-isolation"), coatedMaterial);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto noPrincipledLobes = captureLobeAmounts(0.0f, 0.0f);
    const auto invalidClearcoat = captureLobeAmounts(
        std::numeric_limits<float>::quiet_NaN(), 0.0f);
    const auto invalidSheen = captureLobeAmounts(
        0.0f, std::numeric_limits<float>::quiet_NaN());
    const auto enabledClearcoat = captureLobeAmounts(1.0f, 0.0f);
    const auto enabledSheen = captureLobeAmounts(0.0f, 1.0f);
    const auto enabledClearcoatAndSheen = captureLobeAmounts(1.0f, 1.0f);
    float clearcoatResponse = 0.0f;
    float sheenResponse = 0.0f;
    float combinedLobeResponse = 0.0f;
    float combinedVsClearcoatResponse = 0.0f;
    float combinedVsSheenResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(invalidClearcoat[channel], noPrincipledLobes[channel],
                    1.0e-5f)
            << "non-finite clearcoat should use its zero fallback, channel "
            << channel;
        EXPECT_NEAR(invalidSheen[channel], noPrincipledLobes[channel],
                    1.0e-5f)
            << "non-finite sheen should use its zero fallback, channel "
            << channel;
        clearcoatResponse = std::max(
            clearcoatResponse,
            std::fabs(enabledClearcoat[channel] - noPrincipledLobes[channel]));
        sheenResponse = std::max(
            sheenResponse,
            std::fabs(enabledSheen[channel] - noPrincipledLobes[channel]));
        combinedLobeResponse = std::max(
            combinedLobeResponse,
            std::fabs(enabledClearcoatAndSheen[channel] -
                      noPrincipledLobes[channel]));
        combinedVsClearcoatResponse = std::max(
            combinedVsClearcoatResponse,
            std::fabs(enabledClearcoatAndSheen[channel] -
                      enabledClearcoat[channel]));
        combinedVsSheenResponse = std::max(
            combinedVsSheenResponse,
            std::fabs(enabledClearcoatAndSheen[channel] -
                      enabledSheen[channel]));
        EXPECT_TRUE(std::isfinite(enabledClearcoatAndSheen[channel]))
            << "combined clearcoat and sheen output should remain finite, "
            << "channel " << channel;
        EXPECT_GE(enabledClearcoatAndSheen[channel], 0.0f)
            << "combined clearcoat and sheen radiance should be non-negative, "
            << "channel " << channel;
        const float isolatedLobeEnergyEnvelope =
            noPrincipledLobes[channel] +
            std::fabs(enabledClearcoat[channel] - noPrincipledLobes[channel]) +
            std::fabs(enabledSheen[channel] - noPrincipledLobes[channel]);
        EXPECT_LE(enabledClearcoatAndSheen[channel],
                  isolatedLobeEnergyEnvelope + 0.01f)
            << "combined clearcoat and sheen should stay within the additive "
            << "isolated-lobe energy envelope, channel " << channel;
    }
    EXPECT_GT(clearcoatResponse, 0.001f)
        << "the fixture should respond to a finite clearcoat amount";
    EXPECT_GT(sheenResponse, 0.001f)
        << "the fixture should respond to a finite sheen amount";
    EXPECT_GT(combinedLobeResponse, 0.001f)
        << "the fixture should respond when clearcoat and sheen are both enabled";
    EXPECT_GT(combinedVsClearcoatResponse, 0.001f)
        << "sheen should contribute alongside clearcoat";
    EXPECT_GT(combinedVsSheenResponse, 0.001f)
        << "clearcoat should contribute alongside sheen";
}

TEST_F(RayTracingAcceptanceTest, SelectsPbrMaterialPerTriangleSlot) {
    constexpr std::array<float, 18> vertices{
        -0.9f, -0.9f, 0.0f, 0.0f, -0.9f, 0.0f, -0.45f, 0.9f, 0.0f,
         0.0f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.45f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
    constexpr std::array<std::uint32_t, 2> materialIndices{0, 1};
    constexpr std::array<float, 12> slotUvs{
        0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    constexpr std::array<float, 12> mirroredSlotUvs{
        1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("material-slot-selection", vertexBuffer.RawPtr(), 6,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("material-slot-selection"), vertices.data(), 6,
        indices.data(), 6, nullptr, materialIndices.data(), slotUvs.data(),
        nullptr, mirroredSlotUvs.data()));

    ArtifactCore::RTPbrMaterial leftMaterial;
    leftMaterial.baseColor[0] = 0.0f;
    leftMaterial.baseColor[1] = 0.0f;
    leftMaterial.baseColor[2] = 0.0f;
    leftMaterial.emissionColor[0] = 1.0f;
    leftMaterial.emissionStrength = 1.0f;
    ArtifactCore::RTPbrMaterial rightMaterial = leftMaterial;
    rightMaterial.emissionColor[0] = 0.0f;
    rightMaterial.specular = 0.0f;
    rightMaterial.ior = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto initialRightPixel = pixelAt(24, 16);
    EXPECT_LT(std::max(initialRightPixel[0],
                       std::max(initialRightPixel[1], initialRightPixel[2])),
              0.05f)
        << "the initial second material slot should produce no emission";

    rightMaterial.emissionColor[2] = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("21-pbr-material-slots.png"));

    const auto leftPixel = pixelAt(8, 16);
    const auto rightPixel = pixelAt(24, 16);
    EXPECT_GT(leftPixel[0], leftPixel[2] + 0.5f);
    EXPECT_GT(rightPixel[2], rightPixel[0] + 0.5f);

    QTemporaryDir slotTextureDirectory;
    ASSERT_TRUE(slotTextureDirectory.isValid());
    const auto redTexturePath = slotTextureDirectory.filePath(
        QStringLiteral("slot-red.png"));
    const auto blueTexturePath = slotTextureDirectory.filePath(
        QStringLiteral("slot-blue.png"));
    QImage redTexture(2, 2, QImage::Format_RGBA8888);
    QImage blueTexture(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(redTexture.isNull());
    ASSERT_FALSE(blueTexture.isNull());
    redTexture.fill(QColor(255, 0, 0, 255));
    blueTexture.fill(QColor(0, 0, 255, 255));
    ASSERT_TRUE(redTexture.save(redTexturePath, "PNG"));
    ASSERT_TRUE(blueTexture.save(blueTexturePath, "PNG"));
    const auto anisotropyUvMapPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-anisotropy-uv.png"));
    QImage anisotropyUvMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(anisotropyUvMap.isNull());
    for (int y = 0; y < anisotropyUvMap.height(); ++y) {
        auto* row = anisotropyUvMap.scanLine(y);
        for (int x = 0; x < anisotropyUvMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < anisotropyUvMap.width() / 2 ? 255 : 128;
            pixel[1] = x < anisotropyUvMap.width() / 2 ? 128 : 255;
            pixel[2] = 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(anisotropyUvMap.save(anisotropyUvMapPath, "PNG"));

    leftMaterial.emissionColor[0] = leftMaterial.emissionColor[1] =
        leftMaterial.emissionColor[2] = 1.0f;
    leftMaterial.emissionTexture = ArtifactCore::UniString(redTexturePath);
    rightMaterial.emissionColor[0] = rightMaterial.emissionColor[1] =
        rightMaterial.emissionColor[2] = 1.0f;
    rightMaterial.emissionTexture = ArtifactCore::UniString(blueTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftRedTexturePixel = pixelAt(8, 16);
    const auto rightBlueTexturePixel = pixelAt(24, 16);
    EXPECT_GT(leftRedTexturePixel[0], leftRedTexturePixel[2] + 0.5f);
    EXPECT_GT(rightBlueTexturePixel[2], rightBlueTexturePixel[0] + 0.5f);

    leftMaterial.emissionTexture = ArtifactCore::UniString(blueTexturePath);
    rightMaterial.emissionTexture = ArtifactCore::UniString(redTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftBlueTexturePixel = pixelAt(8, 16);
    const auto rightRedTexturePixel = pixelAt(24, 16);
    EXPECT_GT(leftBlueTexturePixel[2], leftBlueTexturePixel[0] + 0.5f);
    EXPECT_GT(rightRedTexturePixel[0], rightRedTexturePixel[2] + 0.5f);

    const auto slotEmissionUvRampPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-emission-uv-ramp.png"));
    QImage slotEmissionUvRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotEmissionUvRamp.isNull());
    for (int y = 0; y < slotEmissionUvRamp.height(); ++y) {
        auto* row = slotEmissionUvRamp.scanLine(y);
        for (int x = 0; x < slotEmissionUvRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 255 : 0;
            pixel[1] = 0;
            pixel[2] = x < 4 ? 0 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(slotEmissionUvRamp.save(slotEmissionUvRampPath, "PNG"));
    leftMaterial.emissionTexture = ArtifactCore::UniString(slotEmissionUvRampPath);
    leftMaterial.emissionTextureUvSet = 0;
    leftMaterial.emissionTextureUvOffsetU = 0.0f;
    leftMaterial.emissionTextureUvScale = 1.0f;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotEmissionUv0Left = pixelAt(8, 16);
    const auto slotEmissionUv0Right = pixelAt(24, 16);
    EXPECT_GT(slotEmissionUv0Left[0], slotEmissionUv0Left[2] + 0.5f)
        << "slot 0 emission map should sample its red UV0 half";
    EXPECT_GT(slotEmissionUv0Right[2], slotEmissionUv0Right[0] + 0.5f)
        << "slot 1 emission map should sample its blue UV0 half";

    rightMaterial.emissionTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotEmissionUv1Right = pixelAt(24, 16);
    const auto unchangedSlotEmissionUv1Left = pixelAt(8, 16);
    EXPECT_GT(slotEmissionUv1Right[0], slotEmissionUv1Right[2] + 0.5f)
        << "slot 1 emission UV set should mirror its sample into the red texels";
    EXPECT_GT(unchangedSlotEmissionUv1Left[0],
              unchangedSlotEmissionUv1Left[2] + 0.5f)
        << "slot 1 emission UV set should preserve slot 0";

    rightMaterial.emissionTextureUvSet = 0;
    rightMaterial.emissionTextureUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotEmissionOffsetRight = pixelAt(24, 16);
    const auto unchangedSlotEmissionOffsetLeft = pixelAt(8, 16);
    EXPECT_GT(slotEmissionOffsetRight[0], slotEmissionOffsetRight[2] + 0.5f)
        << "slot 1 emission U offset should move its sample into red texels";
    EXPECT_GT(unchangedSlotEmissionOffsetLeft[0],
              unchangedSlotEmissionOffsetLeft[2] + 0.5f)
        << "slot 1 emission U offset should preserve slot 0";

    rightMaterial.emissionTextureUvOffsetU = 0.0f;
    rightMaterial.emissionTextureUvScale = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotEmissionScaleRight = pixelAt(24, 16);
    const auto unchangedSlotEmissionScaleLeft = pixelAt(8, 16);
    EXPECT_GT(slotEmissionScaleRight[0], slotEmissionScaleRight[2] + 0.5f)
        << "slot 1 emission UV scale should move its sample into red texels";
    EXPECT_GT(unchangedSlotEmissionScaleLeft[0],
              unchangedSlotEmissionScaleLeft[2] + 0.5f)
        << "slot 1 emission UV scale should preserve slot 0";
    rightMaterial.emissionTextureUvScale = 1.0f;

    rightMaterial.emissionTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotEmissionRotationRight = pixelAt(24, 16);
    const auto unchangedSlotEmissionRotationLeft = pixelAt(8, 16);
    EXPECT_GT(slotEmissionRotationRight[0], slotEmissionRotationRight[2] + 0.5f)
        << "slot 1 emission UV rotation should move its sample into red texels";
    EXPECT_GT(unchangedSlotEmissionRotationLeft[0],
              unchangedSlotEmissionRotationLeft[2] + 0.5f)
        << "slot 1 emission UV rotation should preserve slot 0";
    rightMaterial.emissionTextureUvRotationDegrees = 0.0f;

    leftMaterial.emissionTexture = ArtifactCore::UniString();
    rightMaterial.emissionTexture = ArtifactCore::UniString();
    leftMaterial.emissionStrength = 0.0f;
    rightMaterial.emissionStrength = 0.0f;
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 1.0f;
    rightMaterial.baseColor[0] = rightMaterial.baseColor[1] =
        rightMaterial.baseColor[2] = 1.0f;
    leftMaterial.roughness = rightMaterial.roughness = 1.0f;
    leftMaterial.baseColorTexture = ArtifactCore::UniString(redTexturePath);
    rightMaterial.baseColorTexture = ArtifactCore::UniString(blueTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftRedBaseColorPixel = pixelAt(8, 16);
    const auto rightBlueBaseColorPixel = pixelAt(24, 16);
    EXPECT_GT(leftRedBaseColorPixel[0], leftRedBaseColorPixel[2] + 0.01f);
    EXPECT_GT(rightBlueBaseColorPixel[2], rightBlueBaseColorPixel[0] + 0.01f);

    leftMaterial.baseColorTexture = ArtifactCore::UniString(blueTexturePath);
    rightMaterial.baseColorTexture = ArtifactCore::UniString(redTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftBlueBaseColorPixel = pixelAt(8, 16);
    const auto rightRedBaseColorPixel = pixelAt(24, 16);
    EXPECT_GT(leftBlueBaseColorPixel[2], leftBlueBaseColorPixel[0] + 0.01f);
    EXPECT_GT(rightRedBaseColorPixel[0], rightRedBaseColorPixel[2] + 0.01f);

    const auto dielectricMapPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-dielectric.png"));
    const auto metallicMapPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-metallic.png"));
    QImage dielectricMap(2, 2, QImage::Format_RGBA8888);
    QImage metallicMap(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(dielectricMap.isNull());
    ASSERT_FALSE(metallicMap.isNull());
    dielectricMap.fill(QColor(255, 255, 0, 255));
    metallicMap.fill(QColor(255, 255, 255, 255));
    ASSERT_TRUE(dielectricMap.save(dielectricMapPath, "PNG"));
    ASSERT_TRUE(metallicMap.save(metallicMapPath, "PNG"));

    leftMaterial.baseColorTexture = ArtifactCore::UniString();
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.metallic = 1.0f;
    leftMaterial.roughness = 0.6f;
    leftMaterial.metallicRoughnessTexture =
        ArtifactCore::UniString(dielectricMapPath);
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto dielectricLeftPixel = pixelAt(8, 16);
    const auto dielectricRightPixel = pixelAt(24, 16);

    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString(metallicMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto metallicLeftPixel = pixelAt(8, 16);
    const auto unchangedRightPixel = pixelAt(24, 16);
    const float leftSlotMetallicDifference = std::max(
        std::fabs(metallicLeftPixel[0] - dielectricLeftPixel[0]),
        std::max(std::fabs(metallicLeftPixel[1] - dielectricLeftPixel[1]),
                 std::fabs(metallicLeftPixel[2] - dielectricLeftPixel[2])));
    EXPECT_GT(leftSlotMetallicDifference, 0.01f)
        << "changing slot 0 metallic data should change its shading";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRightPixel[channel], dielectricRightPixel[channel],
                    0.01f)
            << "changing slot 0 metallic data should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString(dielectricMapPath);
    rightMaterial.metallicRoughnessTexture = ArtifactCore::UniString(metallicMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedLeftPixel = pixelAt(8, 16);
    const auto metallicRightPixel = pixelAt(24, 16);
    const float rightSlotMetallicDifference = std::max(
        std::fabs(metallicRightPixel[0] - dielectricRightPixel[0]),
        std::max(std::fabs(metallicRightPixel[1] - dielectricRightPixel[1]),
                 std::fabs(metallicRightPixel[2] - dielectricRightPixel[2])));
    EXPECT_GT(rightSlotMetallicDifference, 0.01f)
        << "changing slot 1 metallic data should change its shading";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedLeftPixel[channel], dielectricLeftPixel[channel],
                    0.01f)
            << "changing slot 1 metallic data should preserve slot 0, channel "
            << channel;
    }

    const auto slotSurfaceUvRampPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-surface-uv-ramp.png"));
    QImage slotSurfaceUvRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotSurfaceUvRamp.isNull());
    for (int y = 0; y < slotSurfaceUvRamp.height(); ++y) {
        auto* row = slotSurfaceUvRamp.scanLine(y);
        for (int x = 0; x < slotSurfaceUvRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 0 : 255;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(slotSurfaceUvRamp.save(slotSurfaceUvRampPath, "PNG"));
    leftMaterial.emissionTexture = ArtifactCore::UniString();
    rightMaterial.emissionTexture = ArtifactCore::UniString();
    leftMaterial.emissionStrength = rightMaterial.emissionStrength = 0.0f;
    leftMaterial.baseColorTexture = ArtifactCore::UniString();
    rightMaterial.baseColorTexture = ArtifactCore::UniString();
    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString();
    rightMaterial.metallicRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.metallicTexture = rightMaterial.metallicTexture =
        ArtifactCore::UniString(slotSurfaceUvRampPath);
    leftMaterial.roughnessTexture = rightMaterial.roughnessTexture =
        ArtifactCore::UniString(slotSurfaceUvRampPath);
    leftMaterial.metallic = rightMaterial.metallic = 1.0f;
    leftMaterial.roughness = rightMaterial.roughness = 1.0f;
    leftMaterial.metallicTextureUvSet = rightMaterial.metallicTextureUvSet = 0;
    leftMaterial.roughnessTextureUvSet = rightMaterial.roughnessTextureUvSet = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotMetallicRoughnessUv0Left = pixelAt(8, 16);
    const auto slotMetallicRoughnessUv0Right = pixelAt(24, 16);

    rightMaterial.metallicTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightMetallicUv1Pixel = pixelAt(24, 16);
    const auto unchangedMetallicUv0Left = pixelAt(8, 16);
    const float rightMetallicUvRoutingDifference = std::max(
        std::fabs(rightMetallicUv1Pixel[0] - slotMetallicRoughnessUv0Right[0]),
        std::max(std::fabs(rightMetallicUv1Pixel[1] -
                           slotMetallicRoughnessUv0Right[1]),
                 std::fabs(rightMetallicUv1Pixel[2] -
                           slotMetallicRoughnessUv0Right[2])));
    EXPECT_GT(rightMetallicUvRoutingDifference, 0.001f)
        << "slot 1 metallic UV set should affect only its material response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedMetallicUv0Left[channel],
                    slotMetallicRoughnessUv0Left[channel], 0.01f)
            << "slot 1 metallic UV set should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.metallicTextureUvSet = 0;
    rightMaterial.roughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightRoughnessUv1Pixel = pixelAt(24, 16);
    const auto unchangedRoughnessUv0Left = pixelAt(8, 16);
    const float rightRoughnessUvRoutingDifference = std::max(
        std::fabs(rightRoughnessUv1Pixel[0] - slotMetallicRoughnessUv0Right[0]),
        std::max(std::fabs(rightRoughnessUv1Pixel[1] -
                           slotMetallicRoughnessUv0Right[1]),
                 std::fabs(rightRoughnessUv1Pixel[2] -
                           slotMetallicRoughnessUv0Right[2])));
    EXPECT_GT(rightRoughnessUvRoutingDifference, 0.001f)
        << "slot 1 roughness UV set should affect only its material response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRoughnessUv0Left[channel],
                    slotMetallicRoughnessUv0Left[channel], 0.01f)
            << "slot 1 roughness UV set should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.roughnessTextureUvSet = 0;
    rightMaterial.metallicTextureUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightMetallicOffsetPixel = pixelAt(24, 16);
    const auto unchangedMetallicOffsetLeft = pixelAt(8, 16);
    const float rightMetallicOffsetDifference = std::max(
        std::fabs(rightMetallicOffsetPixel[0] - slotMetallicRoughnessUv0Right[0]),
        std::max(std::fabs(rightMetallicOffsetPixel[1] -
                           slotMetallicRoughnessUv0Right[1]),
                 std::fabs(rightMetallicOffsetPixel[2] -
                           slotMetallicRoughnessUv0Right[2])));
    EXPECT_GT(rightMetallicOffsetDifference, 0.001f)
        << "slot 1 metallic U offset should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedMetallicOffsetLeft[channel],
                    slotMetallicRoughnessUv0Left[channel], 0.01f)
            << "slot 1 metallic U offset should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.metallicTextureUvOffsetU = 0.0f;
    rightMaterial.roughnessTextureUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightRoughnessOffsetPixel = pixelAt(24, 16);
    const auto unchangedRoughnessOffsetLeft = pixelAt(8, 16);
    const float rightRoughnessOffsetDifference = std::max(
        std::fabs(rightRoughnessOffsetPixel[0] - slotMetallicRoughnessUv0Right[0]),
        std::max(std::fabs(rightRoughnessOffsetPixel[1] -
                           slotMetallicRoughnessUv0Right[1]),
                 std::fabs(rightRoughnessOffsetPixel[2] -
                           slotMetallicRoughnessUv0Right[2])));
    EXPECT_GT(rightRoughnessOffsetDifference, 0.001f)
        << "slot 1 roughness U offset should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRoughnessOffsetLeft[channel],
                    slotMetallicRoughnessUv0Left[channel], 0.01f)
            << "slot 1 roughness U offset should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.roughnessTextureUvOffsetU = 0.0f;
    rightMaterial.metallicTextureUvScaleU = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightMetallicScalePixel = pixelAt(24, 16);
    const auto unchangedMetallicScaleLeft = pixelAt(8, 16);
    const float rightMetallicScaleDifference = std::max(
        std::fabs(rightMetallicScalePixel[0] - slotMetallicRoughnessUv0Right[0]),
        std::max(std::fabs(rightMetallicScalePixel[1] -
                           slotMetallicRoughnessUv0Right[1]),
                 std::fabs(rightMetallicScalePixel[2] -
                           slotMetallicRoughnessUv0Right[2])));
    EXPECT_GT(rightMetallicScaleDifference, 0.001f)
        << "slot 1 metallic U scale should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedMetallicScaleLeft[channel],
                    slotMetallicRoughnessUv0Left[channel], 0.01f)
            << "slot 1 metallic U scale should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.metallicTextureUvScaleU = 1.0f;
    rightMaterial.roughnessTextureUvScaleU = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightRoughnessScalePixel = pixelAt(24, 16);
    const auto unchangedRoughnessScaleLeft = pixelAt(8, 16);
    const float rightRoughnessScaleDifference = std::max(
        std::fabs(rightRoughnessScalePixel[0] - slotMetallicRoughnessUv0Right[0]),
        std::max(std::fabs(rightRoughnessScalePixel[1] -
                           slotMetallicRoughnessUv0Right[1]),
                 std::fabs(rightRoughnessScalePixel[2] -
                           slotMetallicRoughnessUv0Right[2])));
    EXPECT_GT(rightRoughnessScaleDifference, 0.001f)
        << "slot 1 roughness U scale should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRoughnessScaleLeft[channel],
                    slotMetallicRoughnessUv0Left[channel], 0.01f)
            << "slot 1 roughness U scale should preserve slot 0, channel "
            << channel;
    }
    rightMaterial.metallicTextureUvScaleU = 1.0f;
    rightMaterial.roughnessTextureUvScaleU = 1.0f;

    const auto slotSurfaceVTexturePath = slotTextureDirectory.filePath(
        QStringLiteral("slot-surface-v-ramp.png"));
    QImage slotSurfaceVTexture(2, 8, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotSurfaceVTexture.isNull());
    for (int y = 0; y < slotSurfaceVTexture.height(); ++y) {
        auto* row = slotSurfaceVTexture.scanLine(y);
        for (int x = 0; x < slotSurfaceVTexture.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = y < 4 ? 0 : 255;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(slotSurfaceVTexture.save(slotSurfaceVTexturePath, "PNG"));
    leftMaterial.metallicTexture = rightMaterial.metallicTexture =
        ArtifactCore::UniString(slotSurfaceVTexturePath);
    leftMaterial.roughnessTexture = rightMaterial.roughnessTexture =
        ArtifactCore::UniString(slotSurfaceVTexturePath);
    rightMaterial.metallicTextureUvOffsetV = 0.0f;
    rightMaterial.roughnessTextureUvOffsetV = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotMetallicRoughnessV0Left = pixelAt(8, 16);
    const auto slotMetallicRoughnessV0Right = pixelAt(24, 16);

    rightMaterial.metallicTextureUvOffsetV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightMetallicVOffsetPixel = pixelAt(24, 16);
    const auto unchangedMetallicVOffsetLeft = pixelAt(8, 16);
    const float rightMetallicVOffsetDifference = std::max(
        std::fabs(rightMetallicVOffsetPixel[0] - slotMetallicRoughnessV0Right[0]),
        std::max(std::fabs(rightMetallicVOffsetPixel[1] -
                           slotMetallicRoughnessV0Right[1]),
                 std::fabs(rightMetallicVOffsetPixel[2] -
                           slotMetallicRoughnessV0Right[2])));
    EXPECT_GT(rightMetallicVOffsetDifference, 0.001f)
        << "slot 1 metallic V offset should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedMetallicVOffsetLeft[channel],
                    slotMetallicRoughnessV0Left[channel], 0.01f)
            << "slot 1 metallic V offset should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.metallicTextureUvOffsetV = 0.0f;
    rightMaterial.roughnessTextureUvOffsetV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightRoughnessVOffsetPixel = pixelAt(24, 16);
    const auto unchangedRoughnessVOffsetLeft = pixelAt(8, 16);
    const float rightRoughnessVOffsetDifference = std::max(
        std::fabs(rightRoughnessVOffsetPixel[0] - slotMetallicRoughnessV0Right[0]),
        std::max(std::fabs(rightRoughnessVOffsetPixel[1] -
                           slotMetallicRoughnessV0Right[1]),
                 std::fabs(rightRoughnessVOffsetPixel[2] -
                           slotMetallicRoughnessV0Right[2])));
    EXPECT_GT(rightRoughnessVOffsetDifference, 0.001f)
        << "slot 1 roughness V offset should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRoughnessVOffsetLeft[channel],
                    slotMetallicRoughnessV0Left[channel], 0.01f)
            << "slot 1 roughness V offset should preserve slot 0, channel "
            << channel;
    }
    rightMaterial.roughnessTextureUvOffsetV = 0.0f;

    rightMaterial.metallicTextureUvScaleV = 2.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightMetallicVScalePixel = pixelAt(24, 16);
    const auto unchangedMetallicVScaleLeft = pixelAt(8, 16);
    const float rightMetallicVScaleDifference = std::max(
        std::fabs(rightMetallicVScalePixel[0] - slotMetallicRoughnessV0Right[0]),
        std::max(std::fabs(rightMetallicVScalePixel[1] -
                           slotMetallicRoughnessV0Right[1]),
                 std::fabs(rightMetallicVScalePixel[2] -
                           slotMetallicRoughnessV0Right[2])));
    EXPECT_GT(rightMetallicVScaleDifference, 0.001f)
        << "slot 1 metallic V scale should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedMetallicVScaleLeft[channel],
                    slotMetallicRoughnessV0Left[channel], 0.01f)
            << "slot 1 metallic V scale should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.metallicTextureUvScaleV = 1.0f;
    rightMaterial.roughnessTextureUvScaleV = 2.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rightRoughnessVScalePixel = pixelAt(24, 16);
    const auto unchangedRoughnessVScaleLeft = pixelAt(8, 16);
    const float rightRoughnessVScaleDifference = std::max(
        std::fabs(rightRoughnessVScalePixel[0] - slotMetallicRoughnessV0Right[0]),
        std::max(std::fabs(rightRoughnessVScalePixel[1] -
                           slotMetallicRoughnessV0Right[1]),
                 std::fabs(rightRoughnessVScalePixel[2] -
                           slotMetallicRoughnessV0Right[2])));
    EXPECT_GT(rightRoughnessVScaleDifference, 0.001f)
        << "slot 1 roughness V scale should change its map sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRoughnessVScaleLeft[channel],
                    slotMetallicRoughnessV0Left[channel], 0.01f)
            << "slot 1 roughness V scale should preserve slot 0, channel "
            << channel;
    }
    rightMaterial.roughnessTextureUvScaleV = 1.0f;

    const auto smoothRoughnessPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-roughness-smooth.png"));
    const auto roughRoughnessPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-roughness-rough.png"));
    QImage smoothRoughnessMap(2, 2, QImage::Format_RGBA8888);
    QImage roughRoughnessMap(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(smoothRoughnessMap.isNull());
    ASSERT_FALSE(roughRoughnessMap.isNull());
    smoothRoughnessMap.fill(QColor(51, 51, 51, 255));
    roughRoughnessMap.fill(QColor(255, 255, 255, 255));
    ASSERT_TRUE(smoothRoughnessMap.save(smoothRoughnessPath, "PNG"));
    ASSERT_TRUE(roughRoughnessMap.save(roughRoughnessPath, "PNG"));

    leftMaterial.emissionTexture = ArtifactCore::UniString();
    leftMaterial.emissionStrength = 0.0f;
    leftMaterial.baseColorTexture = ArtifactCore::UniString();
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.metallicTexture = ArtifactCore::UniString();
    leftMaterial.metallic = 0.0f;
    leftMaterial.roughness = 1.0f;
    leftMaterial.roughnessTexture = ArtifactCore::UniString(smoothRoughnessPath);
    leftMaterial.normalTexture = ArtifactCore::UniString();
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatTexture = ArtifactCore::UniString();
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString();
    leftMaterial.sheen = 0.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString();
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString();
    rightMaterial = leftMaterial;
    rightMaterial.roughnessTexture = ArtifactCore::UniString(roughRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto smoothRoughnessLeftPixel = pixelAt(8, 16);
    const auto roughRoughnessRightPixel = pixelAt(24, 16);

    leftMaterial.roughnessTexture = ArtifactCore::UniString(roughRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughRoughnessLeftPixel = pixelAt(8, 16);
    const auto unchangedRoughRoughnessRightPixel = pixelAt(24, 16);
    const float leftRoughnessMapDifference = std::max(
        std::fabs(roughRoughnessLeftPixel[0] - smoothRoughnessLeftPixel[0]),
        std::max(std::fabs(roughRoughnessLeftPixel[1] -
                           smoothRoughnessLeftPixel[1]),
                 std::fabs(roughRoughnessLeftPixel[2] -
                           smoothRoughnessLeftPixel[2])));
    EXPECT_GT(leftRoughnessMapDifference, 0.001f)
        << "slot 0 roughness map should change its specular response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRoughRoughnessRightPixel[channel],
                    roughRoughnessRightPixel[channel], 0.01f)
            << "changing slot 0 roughness map should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.roughnessTexture = ArtifactCore::UniString(smoothRoughnessPath);
    rightMaterial.roughnessTexture = ArtifactCore::UniString(smoothRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedSmoothRoughnessLeftPixel = pixelAt(8, 16);
    const auto smoothRoughnessRightPixel = pixelAt(24, 16);
    const float rightRoughnessMapDifference = std::max(
        std::fabs(smoothRoughnessRightPixel[0] - roughRoughnessRightPixel[0]),
        std::max(std::fabs(smoothRoughnessRightPixel[1] -
                           roughRoughnessRightPixel[1]),
                 std::fabs(smoothRoughnessRightPixel[2] -
                           roughRoughnessRightPixel[2])));
    EXPECT_GT(rightRoughnessMapDifference, 0.001f)
        << "slot 1 roughness map should change its specular response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSmoothRoughnessLeftPixel[channel],
                    smoothRoughnessLeftPixel[channel], 0.01f)
            << "changing slot 1 roughness map should preserve slot 0, channel "
            << channel;
    }
    leftMaterial.roughnessTexture = ArtifactCore::UniString();
    rightMaterial.roughnessTexture = ArtifactCore::UniString();

    const auto flatNormalPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-normal-flat.png"));
    const auto tiltedNormalPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-normal-tilted.png"));
    QImage flatNormal(2, 2, QImage::Format_RGBA8888);
    QImage tiltedNormal(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(flatNormal.isNull());
    ASSERT_FALSE(tiltedNormal.isNull());
    flatNormal.fill(QColor(128, 128, 255, 255));
    tiltedNormal.fill(QColor(204, 128, 230, 255));
    ASSERT_TRUE(flatNormal.save(flatNormalPath, "PNG"));
    ASSERT_TRUE(tiltedNormal.save(tiltedNormalPath, "PNG"));
    const auto slotNormalUvRampPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-normal-uv-ramp.png"));
    QImage slotNormalUvRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotNormalUvRamp.isNull());
    for (int y = 0; y < slotNormalUvRamp.height(); ++y) {
        auto* row = slotNormalUvRamp.scanLine(y);
        for (int x = 0; x < slotNormalUvRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            const bool tilted = x >= 4;
            pixel[0] = tilted ? 204 : 128;
            pixel[1] = 128;
            pixel[2] = tilted ? 230 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(slotNormalUvRamp.save(slotNormalUvRampPath, "PNG"));

    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.metallic = rightMaterial.metallic = 0.0f;
    leftMaterial.roughness = rightMaterial.roughness = 0.5f;
    leftMaterial.baseColorTexture = ArtifactCore::UniString();
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.normalTexture = ArtifactCore::UniString(flatNormalPath);
    leftMaterial.normalStrength = 1.0f;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto flatLeftPixel = pixelAt(8, 16);
    const auto flatRightPixel = pixelAt(24, 16);

    leftMaterial.normalTexture = ArtifactCore::UniString(tiltedNormalPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto tiltedLeftPixel = pixelAt(8, 16);
    const auto unchangedFlatRightPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(tiltedLeftPixel[0] - flatLeftPixel[0]), 0.01f)
        << "slot 0 normal map should change its shading";
    EXPECT_NEAR(unchangedFlatRightPixel[0], flatRightPixel[0], 0.01f)
        << "changing slot 0 normal map should preserve slot 1 shading";

    leftMaterial.normalTexture = ArtifactCore::UniString(flatNormalPath);
    rightMaterial.normalTexture = ArtifactCore::UniString(tiltedNormalPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedFlatLeftPixel = pixelAt(8, 16);
    const auto tiltedRightPixel = pixelAt(24, 16);
    EXPECT_NEAR(unchangedFlatLeftPixel[0], flatLeftPixel[0], 0.01f)
        << "changing slot 1 normal map should preserve slot 0 shading";
    EXPECT_GT(std::fabs(tiltedRightPixel[0] - flatRightPixel[0]), 0.01f)
        << "slot 1 normal map should change its shading";

    leftMaterial.normalTexture = ArtifactCore::UniString(tiltedNormalPath);
    leftMaterial.normalUvSet = 0;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotNormalUv0Left = pixelAt(8, 16);
    const auto slotNormalUv0Right = pixelAt(24, 16);

    rightMaterial.normalUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotNormalUv1Right = pixelAt(24, 16);
    const auto unchangedSlotNormalLeft = pixelAt(8, 16);
    const float slotNormalUvRoutingDifference = std::max(
        std::fabs(slotNormalUv1Right[0] - slotNormalUv0Right[0]),
        std::max(std::fabs(slotNormalUv1Right[1] - slotNormalUv0Right[1]),
                 std::fabs(slotNormalUv1Right[2] - slotNormalUv0Right[2])));
    EXPECT_GT(slotNormalUvRoutingDifference, 0.001f)
        << "slot 1 normal UV set should change its tangent-space shading";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotNormalLeft[channel], slotNormalUv0Left[channel],
                    0.01f)
            << "slot 1 normal UV set should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.normalUvRotationDegrees = 0.0f;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotNormalRotation0Left = pixelAt(8, 16);
    const auto slotNormalRotation0Right = pixelAt(24, 16);

    rightMaterial.normalUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotNormalRotation180Right = pixelAt(24, 16);
    const auto unchangedSlotNormalRotationLeft = pixelAt(8, 16);
    const float slotNormalRotationDifference = std::max(
        std::fabs(slotNormalRotation180Right[0] - slotNormalRotation0Right[0]),
        std::max(std::fabs(slotNormalRotation180Right[1] -
                           slotNormalRotation0Right[1]),
                 std::fabs(slotNormalRotation180Right[2] -
                           slotNormalRotation0Right[2])));
    EXPECT_GT(slotNormalRotationDifference, 0.001f)
        << "slot 1 normal UV rotation should rotate its tangent-space response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotNormalRotationLeft[channel],
                    slotNormalRotation0Left[channel], 0.01f)
            << "slot 1 normal UV rotation should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.normalTexture = ArtifactCore::UniString(slotNormalUvRampPath);
    leftMaterial.normalUvRotationDegrees = 0.0f;
    leftMaterial.normalUvOffsetU = 0.0f;
    leftMaterial.normalUvScale = 1.0f;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotNormalOffset0Left = pixelAt(8, 16);
    const auto slotNormalOffset0Right = pixelAt(24, 16);

    rightMaterial.normalUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotNormalOffsetRight = pixelAt(24, 16);
    const auto unchangedSlotNormalOffsetLeft = pixelAt(8, 16);
    const float slotNormalOffsetDifference = std::max(
        std::fabs(slotNormalOffsetRight[0] - slotNormalOffset0Right[0]),
        std::max(std::fabs(slotNormalOffsetRight[1] - slotNormalOffset0Right[1]),
                 std::fabs(slotNormalOffsetRight[2] - slotNormalOffset0Right[2])));
    EXPECT_GT(slotNormalOffsetDifference, 0.001f)
        << "slot 1 normal UV offset should shift its sampled texels";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotNormalOffsetLeft[channel],
                    slotNormalOffset0Left[channel], 0.01f)
            << "slot 1 normal UV offset should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.normalUvOffsetU = 0.0f;
    rightMaterial.normalUvScale = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotNormalScaleRight = pixelAt(24, 16);
    const auto unchangedSlotNormalScaleLeft = pixelAt(8, 16);
    const float slotNormalScaleDifference = std::max(
        std::fabs(slotNormalScaleRight[0] - slotNormalOffset0Right[0]),
        std::max(std::fabs(slotNormalScaleRight[1] - slotNormalOffset0Right[1]),
                 std::fabs(slotNormalScaleRight[2] - slotNormalOffset0Right[2])));
    EXPECT_GT(slotNormalScaleDifference, 0.001f)
        << "slot 1 normal UV scale should change its sampled texels";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotNormalScaleLeft[channel], slotNormalOffset0Left[channel],
                    0.01f)
            << "slot 1 normal UV scale should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.normalTexture = ArtifactCore::UniString();
    leftMaterial.clearcoat = 1.0f;
    leftMaterial.clearcoatRoughness = 0.2f;
    leftMaterial.clearcoatNormalStrength = 1.0f;
    leftMaterial.clearcoatNormalUvSet = 0;
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString(flatNormalPath);
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto flatCoatLeftPixel = pixelAt(8, 16);
    const auto flatCoatRightPixel = pixelAt(24, 16);

    leftMaterial.clearcoatNormalTexture =
        ArtifactCore::UniString(tiltedNormalPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto tiltedCoatLeftPixel = pixelAt(8, 16);
    const auto unchangedCoatRightPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(tiltedCoatLeftPixel[0] - flatCoatLeftPixel[0]), 0.01f)
        << "slot 0 clearcoat-normal map should change its coated lobe";
    EXPECT_NEAR(unchangedCoatRightPixel[0], flatCoatRightPixel[0], 0.01f)
        << "changing slot 0 clearcoat-normal map should preserve slot 1";

    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString(flatNormalPath);
    rightMaterial.clearcoatNormalTexture = ArtifactCore::UniString(tiltedNormalPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedCoatLeftPixel = pixelAt(8, 16);
    const auto tiltedCoatRightPixel = pixelAt(24, 16);
    EXPECT_NEAR(unchangedCoatLeftPixel[0], flatCoatLeftPixel[0], 0.01f)
        << "changing slot 1 clearcoat-normal map should preserve slot 0";
    EXPECT_GT(std::fabs(tiltedCoatRightPixel[0] - flatCoatRightPixel[0]), 0.01f)
        << "slot 1 clearcoat-normal map should change its coated lobe";

    leftMaterial.clearcoatNormalTexture =
        ArtifactCore::UniString(tiltedNormalPath);
    leftMaterial.clearcoatNormalUvSet = 0;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotCoatNormalUv0Left = pixelAt(8, 16);
    const auto slotCoatNormalUv0Right = pixelAt(24, 16);

    rightMaterial.clearcoatNormalUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotCoatNormalUv1Right = pixelAt(24, 16);
    const auto unchangedSlotCoatNormalLeft = pixelAt(8, 16);
    const float slotCoatNormalUvRoutingDifference = std::max(
        std::fabs(slotCoatNormalUv1Right[0] - slotCoatNormalUv0Right[0]),
        std::max(std::fabs(slotCoatNormalUv1Right[1] - slotCoatNormalUv0Right[1]),
                 std::fabs(slotCoatNormalUv1Right[2] - slotCoatNormalUv0Right[2])));
    EXPECT_GT(slotCoatNormalUvRoutingDifference, 0.001f)
        << "slot 1 clearcoat-normal UV set should change its coated tangent frame";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotCoatNormalLeft[channel],
                    slotCoatNormalUv0Left[channel], 0.01f)
            << "slot 1 clearcoat-normal UV set should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.clearcoatNormalUvRotationDegrees = 0.0f;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotCoatNormalRotation0Left = pixelAt(8, 16);
    const auto slotCoatNormalRotation0Right = pixelAt(24, 16);

    rightMaterial.clearcoatNormalUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotCoatNormalRotation180Right = pixelAt(24, 16);
    const auto unchangedSlotCoatNormalRotationLeft = pixelAt(8, 16);
    const float slotCoatNormalRotationDifference = std::max(
        std::fabs(slotCoatNormalRotation180Right[0] -
                  slotCoatNormalRotation0Right[0]),
        std::max(std::fabs(slotCoatNormalRotation180Right[1] -
                           slotCoatNormalRotation0Right[1]),
                 std::fabs(slotCoatNormalRotation180Right[2] -
                           slotCoatNormalRotation0Right[2])));
    EXPECT_GT(slotCoatNormalRotationDifference, 0.001f)
        << "slot 1 clearcoat-normal UV rotation should rotate its tangent-space response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotCoatNormalRotationLeft[channel],
                    slotCoatNormalRotation0Left[channel], 0.01f)
            << "slot 1 clearcoat-normal UV rotation should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.clearcoatNormalTexture =
        ArtifactCore::UniString(slotNormalUvRampPath);
    leftMaterial.clearcoatNormalUvRotationDegrees = 0.0f;
    leftMaterial.clearcoatNormalUvOffsetU = 0.0f;
    leftMaterial.clearcoatNormalUvScale = 1.0f;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotCoatNormalOffset0Left = pixelAt(8, 16);
    const auto slotCoatNormalOffset0Right = pixelAt(24, 16);

    rightMaterial.clearcoatNormalUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotCoatNormalOffsetRight = pixelAt(24, 16);
    const auto unchangedSlotCoatNormalOffsetLeft = pixelAt(8, 16);
    const float slotCoatNormalOffsetDifference = std::max(
        std::fabs(slotCoatNormalOffsetRight[0] - slotCoatNormalOffset0Right[0]),
        std::max(std::fabs(slotCoatNormalOffsetRight[1] -
                           slotCoatNormalOffset0Right[1]),
                 std::fabs(slotCoatNormalOffsetRight[2] -
                           slotCoatNormalOffset0Right[2])));
    EXPECT_GT(slotCoatNormalOffsetDifference, 0.001f)
        << "slot 1 clearcoat-normal UV offset should shift its sampled texels";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotCoatNormalOffsetLeft[channel],
                    slotCoatNormalOffset0Left[channel], 0.01f)
            << "slot 1 clearcoat-normal UV offset should preserve slot 0, channel "
            << channel;
    }

    rightMaterial.clearcoatNormalUvOffsetU = 0.0f;
    rightMaterial.clearcoatNormalUvScale = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotCoatNormalScaleRight = pixelAt(24, 16);
    const auto unchangedSlotCoatNormalScaleLeft = pixelAt(8, 16);
    const float slotCoatNormalScaleDifference = std::max(
        std::fabs(slotCoatNormalScaleRight[0] - slotCoatNormalOffset0Right[0]),
        std::max(std::fabs(slotCoatNormalScaleRight[1] -
                           slotCoatNormalOffset0Right[1]),
                 std::fabs(slotCoatNormalScaleRight[2] -
                           slotCoatNormalOffset0Right[2])));
    EXPECT_GT(slotCoatNormalScaleDifference, 0.001f)
        << "slot 1 clearcoat-normal UV scale should change its sampled texels";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotCoatNormalScaleLeft[channel],
                    slotCoatNormalOffset0Left[channel], 0.01f)
            << "slot 1 clearcoat-normal UV scale should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString();
    leftMaterial.sheen = 1.0f;
    leftMaterial.sheenRoughness = 0.4f;
    leftMaterial.sheenTint[0] = leftMaterial.sheenTint[1] =
        leftMaterial.sheenTint[2] = 1.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString(redTexturePath);
    rightMaterial = leftMaterial;
    rightMaterial.sheenColorTexture = ArtifactCore::UniString(blueTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto redSheenLeftPixel = pixelAt(8, 16);
    const auto blueSheenRightPixel = pixelAt(24, 16);

    leftMaterial.sheenColorTexture = ArtifactCore::UniString(blueTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto blueSheenLeftPixel = pixelAt(8, 16);
    const auto unchangedBlueSheenRightPixel = pixelAt(24, 16);
    const float leftSheenMapDifference = std::max(
        std::fabs(blueSheenLeftPixel[0] - redSheenLeftPixel[0]),
        std::max(std::fabs(blueSheenLeftPixel[1] - redSheenLeftPixel[1]),
                 std::fabs(blueSheenLeftPixel[2] - redSheenLeftPixel[2])));
    EXPECT_GT(leftSheenMapDifference, 0.001f)
        << "slot 0 sheen-color map should change its sheen response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedBlueSheenRightPixel[channel],
                    blueSheenRightPixel[channel], 0.01f)
            << "changing slot 0 sheen-color map should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.sheenColorTexture = ArtifactCore::UniString(redTexturePath);
    rightMaterial.sheenColorTexture = ArtifactCore::UniString(redTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedRedSheenLeftPixel = pixelAt(8, 16);
    const auto redSheenRightPixel = pixelAt(24, 16);
    const float rightSheenMapDifference = std::max(
        std::fabs(redSheenRightPixel[0] - blueSheenRightPixel[0]),
        std::max(std::fabs(redSheenRightPixel[1] - blueSheenRightPixel[1]),
                 std::fabs(redSheenRightPixel[2] - blueSheenRightPixel[2])));
    EXPECT_GT(rightSheenMapDifference, 0.001f)
        << "slot 1 sheen-color map should change its sheen response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRedSheenLeftPixel[channel],
                    redSheenLeftPixel[channel], 0.01f)
            << "changing slot 1 sheen-color map should preserve slot 0, channel "
            << channel;
    }

    const auto slotSheenUvRampPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-sheen-uv-ramp.png"));
    QImage slotSheenUvRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotSheenUvRamp.isNull());
    for (int y = 0; y < slotSheenUvRamp.height(); ++y) {
        auto* row = slotSheenUvRamp.scanLine(y);
        for (int x = 0; x < slotSheenUvRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 255 : 0;
            pixel[1] = 0;
            pixel[2] = x < 4 ? 0 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(slotSheenUvRamp.save(slotSheenUvRampPath, "PNG"));
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.emissionTexture = ArtifactCore::UniString();
    leftMaterial.emissionStrength = 0.0f;
    leftMaterial.metallic = 0.0f;
    leftMaterial.roughness = 0.5f;
    leftMaterial.normalTexture = ArtifactCore::UniString();
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatTexture = ArtifactCore::UniString();
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString();
    leftMaterial.sheen = 1.0f;
    leftMaterial.sheenRoughness = 0.4f;
    leftMaterial.sheenTint[0] = leftMaterial.sheenTint[1] =
        leftMaterial.sheenTint[2] = 1.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString(slotSheenUvRampPath);
    leftMaterial.sheenColorTextureUvSet = 0;
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString();
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotSheenUv0Left = pixelAt(8, 16);
    const auto slotSheenUv0Right = pixelAt(24, 16);

    rightMaterial.sheenColorTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotSheenUv1Right = pixelAt(24, 16);
    const auto unchangedSlotSheenLeft = pixelAt(8, 16);
    const float slotSheenUvRoutingDifference = std::max(
        std::fabs(slotSheenUv1Right[0] - slotSheenUv0Right[0]),
        std::max(std::fabs(slotSheenUv1Right[1] - slotSheenUv0Right[1]),
                 std::fabs(slotSheenUv1Right[2] - slotSheenUv0Right[2])));
    EXPECT_GT(slotSheenUvRoutingDifference, 0.001f)
        << "slot 1 sheen-color UV set should change its sheen response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotSheenLeft[channel], slotSheenUv0Left[channel],
                    0.01f)
            << "slot 1 sheen-color UV set should preserve slot 0, channel "
            << channel;
    }

    const auto smoothSheenRoughnessPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-sheen-roughness-smooth.png"));
    const auto roughSheenRoughnessPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-sheen-roughness-rough.png"));
    QImage smoothSheenRoughnessMap(2, 2, QImage::Format_RGBA8888);
    QImage roughSheenRoughnessMap(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(smoothSheenRoughnessMap.isNull());
    ASSERT_FALSE(roughSheenRoughnessMap.isNull());
    smoothSheenRoughnessMap.fill(QColor(0, 0, 0, 255));
    roughSheenRoughnessMap.fill(QColor(0, 255, 0, 255));
    ASSERT_TRUE(smoothSheenRoughnessMap.save(smoothSheenRoughnessPath, "PNG"));
    ASSERT_TRUE(roughSheenRoughnessMap.save(roughSheenRoughnessPath, "PNG"));

    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.emissionTexture = ArtifactCore::UniString();
    leftMaterial.emissionStrength = 0.0f;
    leftMaterial.metallic = 0.0f;
    leftMaterial.roughness = 1.0f;
    leftMaterial.normalTexture = ArtifactCore::UniString();
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString();
    leftMaterial.sheen = 1.0f;
    leftMaterial.sheenTint[0] = leftMaterial.sheenTint[1] =
        leftMaterial.sheenTint[2] = 1.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString();
    leftMaterial.sheenRoughness = 1.0f;
    leftMaterial.sheenRoughnessTexture =
        ArtifactCore::UniString(smoothSheenRoughnessPath);
    rightMaterial = leftMaterial;
    rightMaterial.sheenRoughnessTexture =
        ArtifactCore::UniString(roughSheenRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto smoothSheenLeftPixel = pixelAt(8, 16);
    const auto roughSheenRightPixel = pixelAt(24, 16);

    leftMaterial.sheenRoughnessTexture =
        ArtifactCore::UniString(roughSheenRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughSheenLeftPixel = pixelAt(8, 16);
    const auto unchangedRoughSheenRightPixel = pixelAt(24, 16);
    const float leftSheenRoughnessDifference = std::max(
        std::fabs(roughSheenLeftPixel[0] - smoothSheenLeftPixel[0]),
        std::max(std::fabs(roughSheenLeftPixel[1] - smoothSheenLeftPixel[1]),
                 std::fabs(roughSheenLeftPixel[2] - smoothSheenLeftPixel[2])));
    EXPECT_GT(leftSheenRoughnessDifference, 0.001f)
        << "slot 0 sheen roughness map should change its sheen response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRoughSheenRightPixel[channel],
                    roughSheenRightPixel[channel], 0.01f)
            << "changing slot 0 sheen roughness map should preserve slot 1, "
               "channel "
            << channel;
    }

    leftMaterial.sheenRoughnessTexture =
        ArtifactCore::UniString(smoothSheenRoughnessPath);
    rightMaterial.sheenRoughnessTexture =
        ArtifactCore::UniString(smoothSheenRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedSmoothSheenLeftPixel = pixelAt(8, 16);
    const auto smoothSheenRightPixel = pixelAt(24, 16);
    const float rightSheenRoughnessDifference = std::max(
        std::fabs(smoothSheenRightPixel[0] - roughSheenRightPixel[0]),
        std::max(std::fabs(smoothSheenRightPixel[1] - roughSheenRightPixel[1]),
                 std::fabs(smoothSheenRightPixel[2] - roughSheenRightPixel[2])));
    EXPECT_GT(rightSheenRoughnessDifference, 0.001f)
        << "slot 1 sheen roughness map should change its sheen response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSmoothSheenLeftPixel[channel],
                    smoothSheenLeftPixel[channel], 0.01f)
            << "changing slot 1 sheen roughness map should preserve slot 0, "
               "channel "
            << channel;
    }

    const auto slotSheenRoughnessUvRampPath =
        slotTextureDirectory.filePath(
            QStringLiteral("slot-sheen-roughness-uv-ramp.png"));
    QImage slotSheenRoughnessUvRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotSheenRoughnessUvRamp.isNull());
    for (int y = 0; y < slotSheenRoughnessUvRamp.height(); ++y) {
        auto* row = slotSheenRoughnessUvRamp.scanLine(y);
        for (int x = 0; x < slotSheenRoughnessUvRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = pixel[1] = pixel[2] = 255;
            pixel[3] = x < 4 ? 0 : 255;
        }
    }
    ASSERT_TRUE(slotSheenRoughnessUvRamp.save(
        slotSheenRoughnessUvRampPath, "PNG"));
    leftMaterial.sheenRoughnessTexture =
        ArtifactCore::UniString(slotSheenRoughnessUvRampPath);
    leftMaterial.sheenRoughnessTextureUvSet = 0;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotSheenRoughnessUv0Left = pixelAt(8, 16);
    const auto slotSheenRoughnessUv0Right = pixelAt(24, 16);

    rightMaterial.sheenRoughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotSheenRoughnessUv1Right = pixelAt(24, 16);
    const auto unchangedSlotSheenRoughnessLeft = pixelAt(8, 16);
    const float slotSheenRoughnessUvRoutingDifference = std::max(
        std::fabs(slotSheenRoughnessUv1Right[0] -
                  slotSheenRoughnessUv0Right[0]),
        std::max(std::fabs(slotSheenRoughnessUv1Right[1] -
                           slotSheenRoughnessUv0Right[1]),
                 std::fabs(slotSheenRoughnessUv1Right[2] -
                           slotSheenRoughnessUv0Right[2])));
    EXPECT_GT(slotSheenRoughnessUvRoutingDifference, 0.001f)
        << "slot 1 sheen-roughness UV set should change its sheen response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotSheenRoughnessLeft[channel],
                    slotSheenRoughnessUv0Left[channel], 0.01f)
            << "slot 1 sheen-roughness UV set should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.sheen = 0.0f;
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.clearcoat = 1.0f;
    leftMaterial.clearcoatRoughness = 0.2f;
    leftMaterial.clearcoatIOR = 1.0f;
    rightMaterial = leftMaterial;
    rightMaterial.clearcoatIOR = 2.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto lowIorLeftPixel = pixelAt(8, 16);
    const auto highIorRightPixel = pixelAt(24, 16);

    leftMaterial.clearcoatIOR = 2.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto highIorLeftPixel = pixelAt(8, 16);
    const auto unchangedHighIorRightPixel = pixelAt(24, 16);
    const float leftClearcoatIorDifference = std::max(
        std::fabs(highIorLeftPixel[0] - lowIorLeftPixel[0]),
        std::max(std::fabs(highIorLeftPixel[1] - lowIorLeftPixel[1]),
                 std::fabs(highIorLeftPixel[2] - lowIorLeftPixel[2])));
    EXPECT_GT(leftClearcoatIorDifference, 0.001f)
        << "slot 0 clearcoat IOR should change its reflection response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedHighIorRightPixel[channel],
                    highIorRightPixel[channel], 0.01f)
            << "changing slot 0 clearcoat IOR should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.clearcoatIOR = 1.0f;
    rightMaterial.clearcoatIOR = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedLowIorLeftPixel = pixelAt(8, 16);
    const auto lowIorRightPixel = pixelAt(24, 16);
    const float rightClearcoatIorDifference = std::max(
        std::fabs(lowIorRightPixel[0] - highIorRightPixel[0]),
        std::max(std::fabs(lowIorRightPixel[1] - highIorRightPixel[1]),
                 std::fabs(lowIorRightPixel[2] - highIorRightPixel[2])));
    EXPECT_GT(rightClearcoatIorDifference, 0.001f)
        << "slot 1 clearcoat IOR should change its reflection response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedLowIorLeftPixel[channel], lowIorLeftPixel[channel],
                    0.01f)
            << "changing slot 1 clearcoat IOR should preserve slot 0, channel "
            << channel;
    }

    const auto noClearcoatPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-clearcoat-none.png"));
    const auto fullClearcoatPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-clearcoat-full.png"));
    QImage noClearcoatMap(2, 2, QImage::Format_RGBA8888);
    QImage fullClearcoatMap(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(noClearcoatMap.isNull());
    ASSERT_FALSE(fullClearcoatMap.isNull());
    noClearcoatMap.fill(QColor(0, 0, 0, 255));
    fullClearcoatMap.fill(QColor(255, 255, 255, 255));
    ASSERT_TRUE(noClearcoatMap.save(noClearcoatPath, "PNG"));
    ASSERT_TRUE(fullClearcoatMap.save(fullClearcoatPath, "PNG"));

    leftMaterial.clearcoatIOR = 1.5f;
    leftMaterial.clearcoatTexture = ArtifactCore::UniString(noClearcoatPath);
    rightMaterial = leftMaterial;
    rightMaterial.clearcoatTexture = ArtifactCore::UniString(fullClearcoatPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto noClearcoatLeftPixel = pixelAt(8, 16);
    const auto fullClearcoatRightPixel = pixelAt(24, 16);

    leftMaterial.clearcoatTexture =
        ArtifactCore::UniString(fullClearcoatPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto fullClearcoatLeftPixel = pixelAt(8, 16);
    const auto unchangedFullClearcoatRightPixel = pixelAt(24, 16);
    const float leftClearcoatMapDifference = std::max(
        std::fabs(fullClearcoatLeftPixel[0] - noClearcoatLeftPixel[0]),
        std::max(std::fabs(fullClearcoatLeftPixel[1] - noClearcoatLeftPixel[1]),
                 std::fabs(fullClearcoatLeftPixel[2] - noClearcoatLeftPixel[2])));
    EXPECT_GT(leftClearcoatMapDifference, 0.001f)
        << "slot 0 clearcoat map should change its coated response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedFullClearcoatRightPixel[channel],
                    fullClearcoatRightPixel[channel], 0.01f)
            << "changing slot 0 clearcoat map should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.clearcoatTexture = ArtifactCore::UniString(noClearcoatPath);
    rightMaterial.clearcoatTexture = ArtifactCore::UniString(noClearcoatPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedNoClearcoatLeftPixel = pixelAt(8, 16);
    const auto noClearcoatRightPixel = pixelAt(24, 16);
    const float rightClearcoatMapDifference = std::max(
        std::fabs(noClearcoatRightPixel[0] - fullClearcoatRightPixel[0]),
        std::max(std::fabs(noClearcoatRightPixel[1] - fullClearcoatRightPixel[1]),
                 std::fabs(noClearcoatRightPixel[2] - fullClearcoatRightPixel[2])));
    EXPECT_GT(rightClearcoatMapDifference, 0.001f)
        << "slot 1 clearcoat map should change its coated response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedNoClearcoatLeftPixel[channel],
                    noClearcoatLeftPixel[channel], 0.01f)
            << "changing slot 1 clearcoat map should preserve slot 0, channel "
            << channel;
    }

    const auto slotClearcoatUvRampPath = slotTextureDirectory.filePath(
        QStringLiteral("slot-clearcoat-uv-ramp.png"));
    QImage slotClearcoatUvRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotClearcoatUvRamp.isNull());
    for (int y = 0; y < slotClearcoatUvRamp.height(); ++y) {
        auto* row = slotClearcoatUvRamp.scanLine(y);
        for (int x = 0; x < slotClearcoatUvRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            const uchar value = x < 4 ? 0 : 255;
            pixel[0] = pixel[1] = pixel[2] = value;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(slotClearcoatUvRamp.save(slotClearcoatUvRampPath, "PNG"));
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.emissionTexture = ArtifactCore::UniString();
    leftMaterial.emissionStrength = 0.0f;
    leftMaterial.metallic = 0.0f;
    leftMaterial.roughness = 0.5f;
    leftMaterial.normalTexture = ArtifactCore::UniString();
    leftMaterial.clearcoat = 1.0f;
    leftMaterial.clearcoatRoughness = 0.2f;
    leftMaterial.clearcoatIOR = 1.5f;
    leftMaterial.clearcoatTexture =
        ArtifactCore::UniString(slotClearcoatUvRampPath);
    leftMaterial.clearcoatTextureUvSet = 0;
    leftMaterial.clearcoatRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString();
    leftMaterial.sheen = 0.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString();
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString();
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotClearcoatUv0Left = pixelAt(8, 16);
    const auto slotClearcoatUv0Right = pixelAt(24, 16);

    rightMaterial.clearcoatTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotClearcoatUv1Right = pixelAt(24, 16);
    const auto unchangedSlotClearcoatLeft = pixelAt(8, 16);
    const float slotClearcoatUvRoutingDifference = std::max(
        std::fabs(slotClearcoatUv1Right[0] - slotClearcoatUv0Right[0]),
        std::max(std::fabs(slotClearcoatUv1Right[1] - slotClearcoatUv0Right[1]),
                 std::fabs(slotClearcoatUv1Right[2] - slotClearcoatUv0Right[2])));
    EXPECT_GT(slotClearcoatUvRoutingDifference, 0.001f)
        << "slot 1 clearcoat UV set should change its coated response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotClearcoatLeft[channel], slotClearcoatUv0Left[channel],
                    0.01f)
            << "slot 1 clearcoat UV set should preserve slot 0, channel "
            << channel;
    }

    const auto slotClearcoatRoughnessUvRampPath =
        slotTextureDirectory.filePath(
            QStringLiteral("slot-clearcoat-roughness-uv-ramp.png"));
    QImage slotClearcoatRoughnessUvRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(slotClearcoatRoughnessUvRamp.isNull());
    for (int y = 0; y < slotClearcoatRoughnessUvRamp.height(); ++y) {
        auto* row = slotClearcoatRoughnessUvRamp.scanLine(y);
        for (int x = 0; x < slotClearcoatRoughnessUvRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 255;
            pixel[1] = x < 4 ? 0 : 255;
            pixel[2] = 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(slotClearcoatRoughnessUvRamp.save(
        slotClearcoatRoughnessUvRampPath, "PNG"));
    leftMaterial.clearcoatRoughnessTexture =
        ArtifactCore::UniString(slotClearcoatRoughnessUvRampPath);
    leftMaterial.clearcoatRoughnessTextureUvSet = 0;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotClearcoatRoughnessUv0Left = pixelAt(8, 16);
    const auto slotClearcoatRoughnessUv0Right = pixelAt(24, 16);

    rightMaterial.clearcoatRoughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto slotClearcoatRoughnessUv1Right = pixelAt(24, 16);
    const auto unchangedSlotClearcoatRoughnessLeft = pixelAt(8, 16);
    const float slotClearcoatRoughnessUvRoutingDifference = std::max(
        std::fabs(slotClearcoatRoughnessUv1Right[0] -
                  slotClearcoatRoughnessUv0Right[0]),
        std::max(std::fabs(slotClearcoatRoughnessUv1Right[1] -
                           slotClearcoatRoughnessUv0Right[1]),
                 std::fabs(slotClearcoatRoughnessUv1Right[2] -
                           slotClearcoatRoughnessUv0Right[2])));
    EXPECT_GT(slotClearcoatRoughnessUvRoutingDifference, 0.001f)
        << "slot 1 clearcoat-roughness UV set should change its coated response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSlotClearcoatRoughnessLeft[channel],
                    slotClearcoatRoughnessUv0Left[channel], 0.01f)
            << "slot 1 clearcoat-roughness UV set should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.emissionTexture = ArtifactCore::UniString();
    rightMaterial.emissionTexture = ArtifactCore::UniString();
    leftMaterial.emissionStrength = rightMaterial.emissionStrength = 0.0f;
    leftMaterial.baseColorTexture = ArtifactCore::UniString();
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.metallicTexture = ArtifactCore::UniString();
    leftMaterial.roughnessTexture = ArtifactCore::UniString();
    leftMaterial.metallic = rightMaterial.metallic = 0.0f;
    leftMaterial.roughness = rightMaterial.roughness = 0.5f;
    leftMaterial.normalTexture = ArtifactCore::UniString();
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString();
    leftMaterial.clearcoatTexture = ArtifactCore::UniString();
    leftMaterial.clearcoatRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.clearcoatRoughnessTextureUvSet = 0;
    leftMaterial.clearcoat = rightMaterial.clearcoat = 0.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString();
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString();
    leftMaterial.sheen = rightMaterial.sheen = 0.0f;
    leftMaterial.specular = rightMaterial.specular = 0.5f;
    leftMaterial.ior = rightMaterial.ior = 1.5f;
    leftMaterial.specularTint[0] = leftMaterial.specularTint[1] =
        leftMaterial.specularTint[2] = 1.0f;
    rightMaterial = leftMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto whiteSpecularLeftPixel = pixelAt(8, 16);
    const auto whiteSpecularRightPixel = pixelAt(24, 16);

    leftMaterial.specularTint[0] = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto tintedSpecularLeftPixel = pixelAt(8, 16);
    const auto unchangedSpecularRightPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(tintedSpecularLeftPixel[0] - whiteSpecularLeftPixel[0]),
              0.001f)
        << "slot 0 specular tint should change its reflection response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedSpecularRightPixel[channel],
                    whiteSpecularRightPixel[channel], 0.01f)
            << "changing slot 0 specular tint should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.specularTint[0] = 1.0f;
    rightMaterial.specularTint[0] = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedWhiteSpecularLeftPixel = pixelAt(8, 16);
    const auto tintedSpecularRightPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(tintedSpecularRightPixel[0] - whiteSpecularRightPixel[0]),
              0.001f)
        << "slot 1 specular tint should change its reflection response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedWhiteSpecularLeftPixel[channel],
                    whiteSpecularLeftPixel[channel], 0.01f)
            << "changing slot 1 specular tint should preserve slot 0, channel "
            << channel;
    }

    const auto captureMaterialSlotPixels = [&](auto& pixels) {
        if (!manager_->setInstancePbrMaterial(
                ArtifactCore::UniString("material-slot-selection"),
                leftMaterial, 0) ||
            !manager_->setInstancePbrMaterial(
                ArtifactCore::UniString("material-slot-selection"),
                rightMaterial, 1) ||
            !manager_->buildTLAS(context_.RawPtr()) ||
            !manager_->ensurePipelineAndSBT(context_.RawPtr()) ||
            !manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                     kRayTraceTestImageSize)) {
            return false;
        }
        pixels[0] = pixelAt(8, 16);
        pixels[1] = pixelAt(24, 16);
        return true;
    };

    leftMaterial.specularTint[0] = rightMaterial.specularTint[0] = 1.0f;
    leftMaterial.clearcoat = rightMaterial.clearcoat = 1.0f;
    leftMaterial.clearcoatRoughness = rightMaterial.clearcoatRoughness = 0.2f;
    leftMaterial.clearcoatTint[0] = leftMaterial.clearcoatTint[1] =
        leftMaterial.clearcoatTint[2] = 1.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> whiteClearcoatPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(whiteClearcoatPixels));
    leftMaterial.clearcoatTint[0] = 0.0f;
    std::array<std::array<float, 4>, 2> tintedClearcoatLeftPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(tintedClearcoatLeftPixels));
    EXPECT_GT(std::fabs(tintedClearcoatLeftPixels[0][0] -
                        whiteClearcoatPixels[0][0]), 0.001f)
        << "slot 0 clearcoat tint should change its reflection response";
    EXPECT_NEAR(tintedClearcoatLeftPixels[1][0], whiteClearcoatPixels[1][0],
                0.01f)
        << "changing slot 0 clearcoat tint should preserve slot 1";
    leftMaterial.clearcoatTint[0] = 1.0f;
    rightMaterial.clearcoatTint[0] = 0.0f;
    std::array<std::array<float, 4>, 2> tintedClearcoatRightPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(tintedClearcoatRightPixels));
    EXPECT_NEAR(tintedClearcoatRightPixels[0][0], whiteClearcoatPixels[0][0],
                0.01f)
        << "changing slot 1 clearcoat tint should preserve slot 0";
    EXPECT_GT(std::fabs(tintedClearcoatRightPixels[1][0] -
                        whiteClearcoatPixels[1][0]), 0.001f)
        << "slot 1 clearcoat tint should change its reflection response";

    leftMaterial.clearcoat = rightMaterial.clearcoat = 0.0f;
    leftMaterial.sheen = rightMaterial.sheen = 1.0f;
    leftMaterial.sheenRoughness = rightMaterial.sheenRoughness = 0.4f;
    leftMaterial.sheenTint[0] = leftMaterial.sheenTint[1] =
        leftMaterial.sheenTint[2] = 1.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> whiteSheenPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(whiteSheenPixels));
    leftMaterial.sheenTint[0] = 0.0f;
    std::array<std::array<float, 4>, 2> tintedSheenLeftPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(tintedSheenLeftPixels));
    EXPECT_GT(std::fabs(tintedSheenLeftPixels[0][0] - whiteSheenPixels[0][0]),
              0.001f)
        << "slot 0 sheen tint should change its reflection response";
    EXPECT_NEAR(tintedSheenLeftPixels[1][0], whiteSheenPixels[1][0], 0.01f)
        << "changing slot 0 sheen tint should preserve slot 1";
    leftMaterial.sheenTint[0] = 1.0f;
    rightMaterial.sheenTint[0] = 0.0f;
    std::array<std::array<float, 4>, 2> tintedSheenRightPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(tintedSheenRightPixels));
    EXPECT_NEAR(tintedSheenRightPixels[0][0], whiteSheenPixels[0][0], 0.01f)
        << "changing slot 1 sheen tint should preserve slot 0";
    EXPECT_GT(std::fabs(tintedSheenRightPixels[1][0] - whiteSheenPixels[1][0]),
              0.001f)
        << "slot 1 sheen tint should change its reflection response";

    ArtifactCore::RTSceneLight subsurfaceLight;
    subsurfaceLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    subsurfaceLight.directionIntensity[2] = -1.0f;
    subsurfaceLight.directionIntensity[3] = 4.0f;
    subsurfaceLight.colorAttenuationConstant[0] = 1.0f;
    subsurfaceLight.colorAttenuationConstant[1] = 1.0f;
    subsurfaceLight.colorAttenuationConstant[2] = 1.0f;
    subsurfaceLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &subsurfaceLight, 1);
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 1.0f;
    leftMaterial.metallic = 0.0f;
    leftMaterial.specular = 0.0f;
    leftMaterial.emissionColor[0] = leftMaterial.emissionColor[1] =
        leftMaterial.emissionColor[2] = 0.0f;
    leftMaterial.emissionStrength = 0.0f;
    leftMaterial.emissionTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.sheen = 0.0f;
    leftMaterial.subsurfaceColor[0] = leftMaterial.subsurfaceColor[1] =
        leftMaterial.subsurfaceColor[2] = 1.0f;
    leftMaterial.subsurfaceRadius[0] = leftMaterial.subsurfaceRadius[1] =
        leftMaterial.subsurfaceRadius[2] = 1.0f;
    leftMaterial.subsurface = 0.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> noSubsurfacePixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(noSubsurfacePixels));
    leftMaterial.subsurface = 1.0f;
    std::array<std::array<float, 4>, 2> leftSubsurfacePixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftSubsurfacePixels));
    float leftSubsurfaceResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftSubsurfaceResponse = std::max(leftSubsurfaceResponse,
            std::fabs(leftSubsurfacePixels[0][channel] -
                      noSubsurfacePixels[0][channel]));
        EXPECT_NEAR(leftSubsurfacePixels[1][channel],
                    noSubsurfacePixels[1][channel], 0.01f)
            << "changing slot 0 subsurface should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftSubsurfaceResponse, 0.001f)
        << "slot 0 subsurface should change its diffuse response";

    leftMaterial.subsurface = 0.0f;
    rightMaterial.subsurface = 1.0f;
    std::array<std::array<float, 4>, 2> rightSubsurfacePixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightSubsurfacePixels));
    float rightSubsurfaceResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightSubsurfacePixels[0][channel],
                    noSubsurfacePixels[0][channel], 0.01f)
            << "changing slot 1 subsurface should preserve slot 0, channel "
            << channel;
        rightSubsurfaceResponse = std::max(rightSubsurfaceResponse,
            std::fabs(rightSubsurfacePixels[1][channel] -
                      noSubsurfacePixels[1][channel]));
    }
    EXPECT_GT(rightSubsurfaceResponse, 0.001f)
        << "slot 1 subsurface should change its diffuse response";

    leftMaterial.subsurface = rightMaterial.subsurface = 1.0f;
    leftMaterial.subsurfaceAnisotropy = rightMaterial.subsurfaceAnisotropy = 0.0f;
    std::array<std::array<float, 4>, 2> isotropicSubsurfaceSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(isotropicSubsurfaceSlotPixels));
    leftMaterial.subsurfaceAnisotropy = -0.8f;
    std::array<std::array<float, 4>, 2> backwardLeftSubsurfaceSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(backwardLeftSubsurfaceSlotPixels));
    float leftSubsurfaceAnisotropyResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftSubsurfaceAnisotropyResponse = std::max(
            leftSubsurfaceAnisotropyResponse,
            std::fabs(backwardLeftSubsurfaceSlotPixels[0][channel] -
                      isotropicSubsurfaceSlotPixels[0][channel]));
        EXPECT_NEAR(backwardLeftSubsurfaceSlotPixels[1][channel],
                    isotropicSubsurfaceSlotPixels[1][channel], 0.01f)
            << "changing slot 0 subsurface anisotropy should preserve slot 1, "
               "channel "
            << channel;
    }
    EXPECT_GT(leftSubsurfaceAnisotropyResponse, 0.001f)
        << "slot 0 subsurface anisotropy should change its diffuse response";
    leftMaterial.subsurfaceAnisotropy = 0.0f;
    rightMaterial.subsurfaceAnisotropy = 0.8f;
    std::array<std::array<float, 4>, 2> forwardRightSubsurfaceSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(forwardRightSubsurfaceSlotPixels));
    float rightSubsurfaceAnisotropyResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(forwardRightSubsurfaceSlotPixels[0][channel],
                    isotropicSubsurfaceSlotPixels[0][channel], 0.01f)
            << "changing slot 1 subsurface anisotropy should preserve slot 0, "
               "channel "
            << channel;
        rightSubsurfaceAnisotropyResponse = std::max(
            rightSubsurfaceAnisotropyResponse,
            std::fabs(forwardRightSubsurfaceSlotPixels[1][channel] -
                      isotropicSubsurfaceSlotPixels[1][channel]));
    }
    EXPECT_GT(rightSubsurfaceAnisotropyResponse, 0.001f)
        << "slot 1 subsurface anisotropy should change its diffuse response";

    ArtifactCore::RTSceneLight slotDiffuseRoughnessLight;
    slotDiffuseRoughnessLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    slotDiffuseRoughnessLight.directionIntensity[0] = 0.6f;
    slotDiffuseRoughnessLight.directionIntensity[2] = -0.8f;
    slotDiffuseRoughnessLight.directionIntensity[3] = 4.0f;
    slotDiffuseRoughnessLight.colorAttenuationConstant[0] = 1.0f;
    slotDiffuseRoughnessLight.colorAttenuationConstant[1] = 1.0f;
    slotDiffuseRoughnessLight.colorAttenuationConstant[2] = 1.0f;
    slotDiffuseRoughnessLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &slotDiffuseRoughnessLight, 1);
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.7f;
    leftMaterial.metallic = rightMaterial.metallic = 0.0f;
    leftMaterial.specular = rightMaterial.specular = 0.0f;
    leftMaterial.roughness = rightMaterial.roughness = 0.5f;
    leftMaterial.subsurface = rightMaterial.subsurface = 0.0f;
    leftMaterial.diffuseRoughness = rightMaterial.diffuseRoughness = 0.0f;
    leftMaterial.transmission = rightMaterial.transmission = 0.0f;
    leftMaterial.clearcoat = rightMaterial.clearcoat = 0.0f;
    leftMaterial.sheen = rightMaterial.sheen = 0.0f;
    leftMaterial.emissionStrength = rightMaterial.emissionStrength = 0.0f;
    leftMaterial.baseColorTexture = ArtifactCore::UniString{};
    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.metallicTexture = ArtifactCore::UniString{};
    leftMaterial.roughnessTexture = ArtifactCore::UniString{};
    leftMaterial.normalTexture = ArtifactCore::UniString{};
    leftMaterial.occlusionTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
    leftMaterial.sheenColorTexture = ArtifactCore::UniString{};
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.emissionTexture = ArtifactCore::UniString{};
    leftMaterial.anisotropyTexture = ArtifactCore::UniString{};
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> smoothDiffuseRoughnessSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(smoothDiffuseRoughnessSlotPixels));
    leftMaterial.diffuseRoughness = 1.0f;
    std::array<std::array<float, 4>, 2> roughLeftDiffuseSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(roughLeftDiffuseSlotPixels));
    float leftDiffuseRoughnessSlotResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftDiffuseRoughnessSlotResponse = std::max(
            leftDiffuseRoughnessSlotResponse,
            std::fabs(roughLeftDiffuseSlotPixels[0][channel] -
                      smoothDiffuseRoughnessSlotPixels[0][channel]));
        EXPECT_NEAR(roughLeftDiffuseSlotPixels[1][channel],
                    smoothDiffuseRoughnessSlotPixels[1][channel], 0.01f)
            << "changing slot 0 diffuse roughness should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftDiffuseRoughnessSlotResponse, 0.001f)
        << "slot 0 diffuse roughness should change its direct diffuse response";
    leftMaterial.diffuseRoughness = 0.0f;
    rightMaterial.diffuseRoughness = 1.0f;
    std::array<std::array<float, 4>, 2> roughRightDiffuseSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(roughRightDiffuseSlotPixels));
    float rightDiffuseRoughnessSlotResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(roughRightDiffuseSlotPixels[0][channel],
                    smoothDiffuseRoughnessSlotPixels[0][channel], 0.01f)
            << "changing slot 1 diffuse roughness should preserve slot 0, channel "
            << channel;
        rightDiffuseRoughnessSlotResponse = std::max(
            rightDiffuseRoughnessSlotResponse,
            std::fabs(roughRightDiffuseSlotPixels[1][channel] -
                      smoothDiffuseRoughnessSlotPixels[1][channel]));
    }
    EXPECT_GT(rightDiffuseRoughnessSlotResponse, 0.001f)
        << "slot 1 diffuse roughness should change its direct diffuse response";

    leftMaterial.diffuseRoughness = rightMaterial.diffuseRoughness = 0.0f;
    leftMaterial.clearcoat = rightMaterial.clearcoat = 0.0f;
    leftMaterial.sheen = rightMaterial.sheen = 0.0f;
    leftMaterial.clearcoatRoughness = rightMaterial.clearcoatRoughness = 0.2f;
    leftMaterial.sheenRoughness = rightMaterial.sheenRoughness = 0.25f;
    leftMaterial.clearcoatTint[0] = leftMaterial.clearcoatTint[1] =
        leftMaterial.clearcoatTint[2] = 1.0f;
    leftMaterial.sheenTint[0] = leftMaterial.sheenTint[1] =
        leftMaterial.sheenTint[2] = 1.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> noLayeredLobesSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(noLayeredLobesSlotPixels));
    leftMaterial.clearcoat = 1.0f;
    leftMaterial.sheen = 1.0f;
    std::array<std::array<float, 4>, 2> leftLayeredLobesSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftLayeredLobesSlotPixels));
    float leftLayeredLobesResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftLayeredLobesResponse = std::max(
            leftLayeredLobesResponse,
            std::fabs(leftLayeredLobesSlotPixels[0][channel] -
                      noLayeredLobesSlotPixels[0][channel]));
        EXPECT_NEAR(leftLayeredLobesSlotPixels[1][channel],
                    noLayeredLobesSlotPixels[1][channel], 0.01f)
            << "enabling clearcoat and sheen on slot 0 should preserve slot 1, "
               "channel "
            << channel;
    }
    EXPECT_GT(leftLayeredLobesResponse, 0.001f)
        << "slot 0 clearcoat/sheen combination should change its response";
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.sheen = 0.0f;
    rightMaterial.clearcoat = 1.0f;
    rightMaterial.sheen = 1.0f;
    std::array<std::array<float, 4>, 2> rightLayeredLobesSlotPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightLayeredLobesSlotPixels));
    float rightLayeredLobesResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightLayeredLobesSlotPixels[0][channel],
                    noLayeredLobesSlotPixels[0][channel], 0.01f)
            << "enabling clearcoat and sheen on slot 1 should preserve slot 0, "
               "channel "
            << channel;
        rightLayeredLobesResponse = std::max(
            rightLayeredLobesResponse,
            std::fabs(rightLayeredLobesSlotPixels[1][channel] -
                      noLayeredLobesSlotPixels[1][channel]));
    }
    EXPECT_GT(rightLayeredLobesResponse, 0.001f)
        << "slot 1 clearcoat/sheen combination should change its response";
    leftMaterial.clearcoat = rightMaterial.clearcoat = 0.0f;
    leftMaterial.sheen = rightMaterial.sheen = 0.0f;
    manager_->setSceneLights(context_.RawPtr(), &subsurfaceLight, 1);

    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 1.0f;
    leftMaterial.baseColorTexture = ArtifactCore::UniString{};
    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.metallicTexture = ArtifactCore::UniString{};
    leftMaterial.roughnessTexture = ArtifactCore::UniString{};
    leftMaterial.normalTexture = ArtifactCore::UniString{};
    leftMaterial.metallic = 0.0f;
    leftMaterial.specular = 0.0f;
    leftMaterial.roughness = 1.0f;
    leftMaterial.anisotropy = 0.0f;
    leftMaterial.anisotropyTexture = ArtifactCore::UniString{};
    leftMaterial.transmission = 0.0f;
    leftMaterial.subsurface = 0.0f;
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
    leftMaterial.sheen = 0.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString{};
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.emissionStrength = 0.0f;
    leftMaterial.emissionTexture = ArtifactCore::UniString{};
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> whiteBaseColorPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(whiteBaseColorPixels));
    leftMaterial.baseColor[1] = leftMaterial.baseColor[2] = 0.0f;
    std::array<std::array<float, 4>, 2> redLeftBaseColorPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(redLeftBaseColorPixels));
    float leftBaseColorResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftBaseColorResponse = std::max(leftBaseColorResponse,
            std::fabs(redLeftBaseColorPixels[0][channel] -
                      whiteBaseColorPixels[0][channel]));
        EXPECT_NEAR(redLeftBaseColorPixels[1][channel],
                    whiteBaseColorPixels[1][channel], 0.01f)
            << "changing slot 0 base color should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftBaseColorResponse, 0.001f)
        << "slot 0 base color should change its diffuse response";
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 1.0f;
    rightMaterial.baseColor[0] = rightMaterial.baseColor[1] = 0.0f;
    std::array<std::array<float, 4>, 2> blueRightBaseColorPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(blueRightBaseColorPixels));
    float rightBaseColorResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(blueRightBaseColorPixels[0][channel],
                    whiteBaseColorPixels[0][channel], 0.01f)
            << "changing slot 1 base color should preserve slot 0, channel "
            << channel;
        rightBaseColorResponse = std::max(rightBaseColorResponse,
            std::fabs(blueRightBaseColorPixels[1][channel] -
                      whiteBaseColorPixels[1][channel]));
    }
    EXPECT_GT(rightBaseColorResponse, 0.001f)
        << "slot 1 base color should change its diffuse response";

    leftMaterial.subsurface = 0.0f;
    leftMaterial.specular = 0.0f;
    leftMaterial.specularTint[0] = leftMaterial.specularTint[1] =
        leftMaterial.specularTint[2] = 1.0f;
    leftMaterial.metallic = 0.0f;
    leftMaterial.roughness = 0.35f;
    leftMaterial.ior = 1.5f;
    leftMaterial.transmission = 0.0f;
    leftMaterial.emissionStrength = 0.0f;
    leftMaterial.emissionTexture = ArtifactCore::UniString{};
    leftMaterial.baseColorTexture = ArtifactCore::UniString{};
    leftMaterial.metallicRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.metallicTexture = ArtifactCore::UniString{};
    leftMaterial.roughnessTexture = ArtifactCore::UniString{};
    leftMaterial.normalTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
    leftMaterial.sheen = 0.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString{};
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.anisotropy = 0.0f;
    leftMaterial.anisotropyTexture = ArtifactCore::UniString{};
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> noSlotSpecularPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(noSlotSpecularPixels));
    leftMaterial.specular = 1.0f;
    std::array<std::array<float, 4>, 2> leftSlotSpecularPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftSlotSpecularPixels));
    float leftSlotSpecularResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftSlotSpecularResponse = std::max(leftSlotSpecularResponse,
            std::fabs(leftSlotSpecularPixels[0][channel] -
                      noSlotSpecularPixels[0][channel]));
        EXPECT_NEAR(leftSlotSpecularPixels[1][channel],
                    noSlotSpecularPixels[1][channel], 0.01f)
            << "changing slot 0 specular should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftSlotSpecularResponse, 0.001f)
        << "slot 0 specular should change its dielectric reflection";
    leftMaterial.specular = 0.0f;
    rightMaterial.specular = 1.0f;
    std::array<std::array<float, 4>, 2> rightSlotSpecularPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightSlotSpecularPixels));
    float rightSlotSpecularResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightSlotSpecularPixels[0][channel],
                    noSlotSpecularPixels[0][channel], 0.01f)
            << "changing slot 1 specular should preserve slot 0, channel "
            << channel;
        rightSlotSpecularResponse = std::max(rightSlotSpecularResponse,
            std::fabs(rightSlotSpecularPixels[1][channel] -
                      noSlotSpecularPixels[1][channel]));
    }
    EXPECT_GT(rightSlotSpecularResponse, 0.001f)
        << "slot 1 specular should change its dielectric reflection";

    leftMaterial.subsurface = 0.0f;
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatRoughness = 0.2f;
    leftMaterial.clearcoatTint[0] = leftMaterial.clearcoatTint[1] =
        leftMaterial.clearcoatTint[2] = 1.0f;
    leftMaterial.specular = 0.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> noSlotClearcoatPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(noSlotClearcoatPixels));
    leftMaterial.clearcoat = 1.0f;
    std::array<std::array<float, 4>, 2> leftSlotClearcoatPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftSlotClearcoatPixels));
    float leftSlotClearcoatResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftSlotClearcoatResponse = std::max(leftSlotClearcoatResponse,
            std::fabs(leftSlotClearcoatPixels[0][channel] -
                      noSlotClearcoatPixels[0][channel]));
        EXPECT_NEAR(leftSlotClearcoatPixels[1][channel],
                    noSlotClearcoatPixels[1][channel], 0.01f)
            << "changing slot 0 clearcoat should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftSlotClearcoatResponse, 0.001f)
        << "slot 0 clearcoat should change its reflection response";
    leftMaterial.clearcoat = 0.0f;
    rightMaterial.clearcoat = 1.0f;
    std::array<std::array<float, 4>, 2> rightSlotClearcoatPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightSlotClearcoatPixels));
    float rightSlotClearcoatResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightSlotClearcoatPixels[0][channel],
                    noSlotClearcoatPixels[0][channel], 0.01f)
            << "changing slot 1 clearcoat should preserve slot 0, channel "
            << channel;
        rightSlotClearcoatResponse = std::max(rightSlotClearcoatResponse,
            std::fabs(rightSlotClearcoatPixels[1][channel] -
                      noSlotClearcoatPixels[1][channel]));
    }
    EXPECT_GT(rightSlotClearcoatResponse, 0.001f)
        << "slot 1 clearcoat should change its reflection response";

    leftMaterial.clearcoat = rightMaterial.clearcoat = 1.0f;
    leftMaterial.clearcoatRoughness = rightMaterial.clearcoatRoughness = 0.2f;
    std::array<std::array<float, 4>, 2> smoothSlotClearcoatPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(smoothSlotClearcoatPixels));
    leftMaterial.clearcoatRoughness = 1.0f;
    std::array<std::array<float, 4>, 2> roughLeftSlotClearcoatPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(roughLeftSlotClearcoatPixels));
    float leftSlotClearcoatRoughnessResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftSlotClearcoatRoughnessResponse = std::max(
            leftSlotClearcoatRoughnessResponse,
            std::fabs(roughLeftSlotClearcoatPixels[0][channel] -
                      smoothSlotClearcoatPixels[0][channel]));
        EXPECT_NEAR(roughLeftSlotClearcoatPixels[1][channel],
                    smoothSlotClearcoatPixels[1][channel], 0.01f)
            << "changing slot 0 clearcoat roughness should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftSlotClearcoatRoughnessResponse, 0.001f)
        << "slot 0 clearcoat roughness should change its reflection response";
    leftMaterial.clearcoatRoughness = 0.2f;
    rightMaterial.clearcoatRoughness = 1.0f;
    std::array<std::array<float, 4>, 2> roughRightSlotClearcoatPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(roughRightSlotClearcoatPixels));
    float rightSlotClearcoatRoughnessResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(roughRightSlotClearcoatPixels[0][channel],
                    smoothSlotClearcoatPixels[0][channel], 0.01f)
            << "changing slot 1 clearcoat roughness should preserve slot 0, channel "
            << channel;
        rightSlotClearcoatRoughnessResponse = std::max(
            rightSlotClearcoatRoughnessResponse,
            std::fabs(roughRightSlotClearcoatPixels[1][channel] -
                      smoothSlotClearcoatPixels[1][channel]));
    }
    EXPECT_GT(rightSlotClearcoatRoughnessResponse, 0.001f)
        << "slot 1 clearcoat roughness should change its reflection response";

    leftMaterial.clearcoat = 0.0f;
    leftMaterial.sheen = 0.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString{};
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.sheenRoughness = 0.4f;
    leftMaterial.sheenTint[0] = leftMaterial.sheenTint[1] =
        leftMaterial.sheenTint[2] = 1.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> noSlotSheenPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(noSlotSheenPixels));
    leftMaterial.sheen = 1.0f;
    std::array<std::array<float, 4>, 2> leftSlotSheenPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftSlotSheenPixels));
    float leftSlotSheenResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftSlotSheenResponse = std::max(leftSlotSheenResponse,
            std::fabs(leftSlotSheenPixels[0][channel] -
                      noSlotSheenPixels[0][channel]));
        EXPECT_NEAR(leftSlotSheenPixels[1][channel],
                    noSlotSheenPixels[1][channel], 0.01f)
            << "changing slot 0 sheen should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftSlotSheenResponse, 0.001f)
        << "slot 0 sheen should change its reflection response";
    leftMaterial.sheen = 0.0f;
    rightMaterial.sheen = 1.0f;
    std::array<std::array<float, 4>, 2> rightSlotSheenPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightSlotSheenPixels));
    float rightSlotSheenResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightSlotSheenPixels[0][channel],
                    noSlotSheenPixels[0][channel], 0.01f)
            << "changing slot 1 sheen should preserve slot 0, channel "
            << channel;
        rightSlotSheenResponse = std::max(rightSlotSheenResponse,
            std::fabs(rightSlotSheenPixels[1][channel] -
                      noSlotSheenPixels[1][channel]));
    }
    EXPECT_GT(rightSlotSheenResponse, 0.001f)
        << "slot 1 sheen should change its reflection response";

    leftMaterial.sheen = rightMaterial.sheen = 1.0f;
    leftMaterial.sheenRoughness = rightMaterial.sheenRoughness = 0.2f;
    std::array<std::array<float, 4>, 2> smoothSlotSheenPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(smoothSlotSheenPixels));
    leftMaterial.sheenRoughness = 1.0f;
    std::array<std::array<float, 4>, 2> roughLeftSlotSheenPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(roughLeftSlotSheenPixels));
    float leftSlotSheenRoughnessResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftSlotSheenRoughnessResponse = std::max(
            leftSlotSheenRoughnessResponse,
            std::fabs(roughLeftSlotSheenPixels[0][channel] -
                      smoothSlotSheenPixels[0][channel]));
        EXPECT_NEAR(roughLeftSlotSheenPixels[1][channel],
                    smoothSlotSheenPixels[1][channel], 0.01f)
            << "changing slot 0 sheen roughness should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftSlotSheenRoughnessResponse, 0.001f)
        << "slot 0 sheen roughness should change its reflection response";
    leftMaterial.sheenRoughness = 0.2f;
    rightMaterial.sheenRoughness = 1.0f;
    std::array<std::array<float, 4>, 2> roughRightSlotSheenPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(roughRightSlotSheenPixels));
    float rightSlotSheenRoughnessResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(roughRightSlotSheenPixels[0][channel],
                    smoothSlotSheenPixels[0][channel], 0.01f)
            << "changing slot 1 sheen roughness should preserve slot 0, channel "
            << channel;
        rightSlotSheenRoughnessResponse = std::max(
            rightSlotSheenRoughnessResponse,
            std::fabs(roughRightSlotSheenPixels[1][channel] -
                      smoothSlotSheenPixels[1][channel]));
    }
    EXPECT_GT(rightSlotSheenRoughnessResponse, 0.001f)
        << "slot 1 sheen roughness should change its reflection response";

    leftMaterial.subsurface = 0.0f;
    leftMaterial.clearcoat = 0.0f;
    leftMaterial.clearcoatTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.clearcoatNormalTexture = ArtifactCore::UniString{};
    leftMaterial.sheen = 0.0f;
    leftMaterial.sheenColorTexture = ArtifactCore::UniString{};
    leftMaterial.sheenRoughnessTexture = ArtifactCore::UniString{};
    leftMaterial.specular = 0.5f;
    leftMaterial.roughness = 0.35f;
    leftMaterial.ior = 1.5f;
    leftMaterial.anisotropy = 0.0f;
    leftMaterial.anisotropyTexture = ArtifactCore::UniString{};
    leftMaterial.anisotropyRotationDegrees = 0.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> isotropicPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(isotropicPixels));
    leftMaterial.anisotropy = 1.0f;
    std::array<std::array<float, 4>, 2> leftAnisotropyPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftAnisotropyPixels));
    float leftAnisotropyResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftAnisotropyResponse = std::max(leftAnisotropyResponse,
            std::fabs(leftAnisotropyPixels[0][channel] -
                      isotropicPixels[0][channel]));
        EXPECT_NEAR(leftAnisotropyPixels[1][channel],
                    isotropicPixels[1][channel], 0.01f)
            << "changing slot 0 anisotropy should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftAnisotropyResponse, 0.001f)
        << "slot 0 anisotropy should change its specular response";

    leftMaterial.anisotropy = 0.0f;
    rightMaterial.anisotropy = 1.0f;
    std::array<std::array<float, 4>, 2> rightAnisotropyPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightAnisotropyPixels));
    float rightAnisotropyResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightAnisotropyPixels[0][channel],
                    isotropicPixels[0][channel], 0.01f)
            << "changing slot 1 anisotropy should preserve slot 0, channel "
            << channel;
        rightAnisotropyResponse = std::max(rightAnisotropyResponse,
            std::fabs(rightAnisotropyPixels[1][channel] -
                      isotropicPixels[1][channel]));
    }
    EXPECT_GT(rightAnisotropyResponse, 0.001f)
        << "slot 1 anisotropy should change its specular response";

    leftMaterial.anisotropy = 1.0f;
    leftMaterial.anisotropyTexture = ArtifactCore::UniString{};
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> untexturedAnisotropyPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(untexturedAnisotropyPixels));
    leftMaterial.anisotropyTexture = ArtifactCore::UniString(blueTexturePath);
    std::array<std::array<float, 4>, 2> leftAnisotropyTexturePixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftAnisotropyTexturePixels));
    float leftAnisotropyTextureResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftAnisotropyTextureResponse = std::max(
            leftAnisotropyTextureResponse,
            std::fabs(leftAnisotropyTexturePixels[0][channel] -
                      untexturedAnisotropyPixels[0][channel]));
        EXPECT_NEAR(leftAnisotropyTexturePixels[1][channel],
                    untexturedAnisotropyPixels[1][channel], 0.01f)
            << "changing slot 0 anisotropy texture should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftAnisotropyTextureResponse, 0.001f)
        << "slot 0 anisotropy texture should change its specular response";

    leftMaterial.anisotropyTexture = ArtifactCore::UniString{};
    rightMaterial.anisotropyTexture = ArtifactCore::UniString(blueTexturePath);
    std::array<std::array<float, 4>, 2> rightAnisotropyTexturePixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightAnisotropyTexturePixels));
    float rightAnisotropyTextureResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightAnisotropyTexturePixels[0][channel],
                    untexturedAnisotropyPixels[0][channel], 0.01f)
            << "changing slot 1 anisotropy texture should preserve slot 0, channel "
            << channel;
        rightAnisotropyTextureResponse = std::max(
            rightAnisotropyTextureResponse,
            std::fabs(rightAnisotropyTexturePixels[1][channel] -
                      untexturedAnisotropyPixels[1][channel]));
    }
    EXPECT_GT(rightAnisotropyTextureResponse, 0.001f)
        << "slot 1 anisotropy texture should change its specular response";

    leftMaterial.anisotropyTexture = ArtifactCore::UniString{};
    leftMaterial.anisotropy = 1.0f;
    leftMaterial.anisotropyRotationDegrees = 0.0f;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> unrotatedAnisotropyPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(unrotatedAnisotropyPixels));
    leftMaterial.anisotropyRotationDegrees = 90.0f;
    std::array<std::array<float, 4>, 2> leftRotatedAnisotropyPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(leftRotatedAnisotropyPixels));
    float leftAnisotropyRotationResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        leftAnisotropyRotationResponse = std::max(
            leftAnisotropyRotationResponse,
            std::fabs(leftRotatedAnisotropyPixels[0][channel] -
                      unrotatedAnisotropyPixels[0][channel]));
        EXPECT_NEAR(leftRotatedAnisotropyPixels[1][channel],
                    unrotatedAnisotropyPixels[1][channel], 0.01f)
            << "rotating slot 0 anisotropy should preserve slot 1, channel "
            << channel;
    }
    EXPECT_GT(leftAnisotropyRotationResponse, 0.001f)
        << "slot 0 anisotropy rotation should change its specular response";

    leftMaterial.anisotropyRotationDegrees = 0.0f;
    rightMaterial.anisotropyRotationDegrees = 90.0f;
    std::array<std::array<float, 4>, 2> rightRotatedAnisotropyPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(rightRotatedAnisotropyPixels));
    float rightAnisotropyRotationResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rightRotatedAnisotropyPixels[0][channel],
                    unrotatedAnisotropyPixels[0][channel], 0.01f)
            << "rotating slot 1 anisotropy should preserve slot 0, channel "
            << channel;
        rightAnisotropyRotationResponse = std::max(
            rightAnisotropyRotationResponse,
            std::fabs(rightRotatedAnisotropyPixels[1][channel] -
                      unrotatedAnisotropyPixels[1][channel]));
    }
    EXPECT_GT(rightAnisotropyRotationResponse, 0.001f)
        << "slot 1 anisotropy rotation should change its specular response";

    leftMaterial.anisotropyTexture =
        ArtifactCore::UniString(anisotropyUvMapPath);
    leftMaterial.anisotropyTextureUvSet = 0;
    rightMaterial = leftMaterial;
    std::array<std::array<float, 4>, 2> slotUv0Pixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(slotUv0Pixels));
    const float slotUv0Difference = std::max(
        std::fabs(slotUv0Pixels[0][0] - slotUv0Pixels[1][0]),
        std::max(std::fabs(slotUv0Pixels[0][1] - slotUv0Pixels[1][1]),
                 std::fabs(slotUv0Pixels[0][2] - slotUv0Pixels[1][2])));
    EXPECT_GT(slotUv0Difference, 0.02f)
        << "anisotropy UV0 map should distinguish material slots";

    rightMaterial.anisotropyTextureUvSet = 1;
    std::array<std::array<float, 4>, 2> mixedSlotUvPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(mixedSlotUvPixels));
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(mixedSlotUvPixels[0][channel], slotUv0Pixels[0][channel],
                    0.04f)
            << "slot 1 UV selection should preserve slot 0, channel " << channel;
        EXPECT_NEAR(mixedSlotUvPixels[1][channel], slotUv0Pixels[0][channel],
                    0.04f)
            << "slot 1 UV1 should mirror slot 0 UV0, channel " << channel;
    }

    leftMaterial.anisotropyTextureUvSet = 1;
    rightMaterial.anisotropyTextureUvSet = 0;
    std::array<std::array<float, 4>, 2> reversedSlotUvPixels{};
    ASSERT_TRUE(captureMaterialSlotPixels(reversedSlotUvPixels));
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(reversedSlotUvPixels[0][channel], slotUv0Pixels[1][channel],
                    0.04f)
            << "slot 0 UV1 should mirror slot 1 UV0, channel " << channel;
        EXPECT_NEAR(reversedSlotUvPixels[1][channel], slotUv0Pixels[0][channel],
                    0.04f)
            << "slot 0 UV selection should preserve slot 1, channel " << channel;
    }
}

TEST_F(RayTracingAcceptanceTest, SelectsMaskedOpacityTexturePerMaterialSlot) {
    constexpr std::array<float, 24> frontVertices{
        -0.9f, -0.9f, 0.4f, 0.0f, -0.9f, 0.4f,
         0.0f,  0.9f, 0.4f, -0.9f,  0.9f, 0.4f,
         0.0f, -0.9f, 0.4f, 0.9f, -0.9f, 0.4f,
         0.9f,  0.9f, 0.4f, 0.0f,  0.9f, 0.4f};
    constexpr std::array<std::uint32_t, 12> frontIndices{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    constexpr std::array<std::uint32_t, 4> materialIndices{0, 0, 1, 1};
    constexpr std::array<float, 16> frontUvs{
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 16> mirroredFrontUvs1{
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    constexpr std::array<float, 12> backVertices{
        -0.9f, -0.9f, -0.4f, 0.9f, -0.9f, -0.4f,
         0.9f,  0.9f, -0.4f, -0.9f, 0.9f, -0.4f};
    constexpr std::array<std::uint32_t, 6> backIndices{0, 1, 2, 0, 2, 3};
    auto frontVertexBuffer = makeBuffer(
        frontVertices.data(), sizeof(frontVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto frontIndexBuffer = makeBuffer(
        frontIndices.data(), sizeof(frontIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto backVertexBuffer = makeBuffer(
        backVertices.data(), sizeof(backVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto backIndexBuffer = makeBuffer(
        backIndices.data(), sizeof(backIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(frontVertexBuffer, nullptr);
    ASSERT_NE(frontIndexBuffer, nullptr);
    ASSERT_NE(backVertexBuffer, nullptr);
    ASSERT_NE(backIndexBuffer, nullptr);
    addGeometry("masked-opacity-material-slots", frontVertexBuffer.RawPtr(), 8,
                frontIndexBuffer.RawPtr(), 12, float4x4::Identity(), 1, 1,
                frontVertices.data(), frontIndices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        frontVertices.data(), 8, frontIndices.data(), 12, nullptr,
        materialIndices.data(), frontUvs.data(), nullptr,
        mirroredFrontUvs1.data()));
    addGeometry("masked-opacity-slot-background", backVertexBuffer.RawPtr(), 4,
                backIndexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                backVertices.data(), backIndices.data());

    QTemporaryDir opacityDirectory;
    ASSERT_TRUE(opacityDirectory.isValid());
    const auto transparentPath = opacityDirectory.filePath(
        QStringLiteral("transparent.png"));
    const auto opaquePath = opacityDirectory.filePath(
        QStringLiteral("opaque.png"));
    const auto halfAlphaPath = opacityDirectory.filePath(
        QStringLiteral("half-alpha.png"));
    const auto uvSetRampPath = opacityDirectory.filePath(
        QStringLiteral("uv-set-ramp.png"));
    QImage transparentImage(2, 2, QImage::Format_RGBA8888);
    QImage opaqueImage(2, 2, QImage::Format_RGBA8888);
    QImage halfAlphaImage(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(transparentImage.isNull());
    ASSERT_FALSE(opaqueImage.isNull());
    ASSERT_FALSE(halfAlphaImage.isNull());
    transparentImage.fill(QColor(255, 255, 255, 0));
    opaqueImage.fill(QColor(255, 255, 255, 255));
    halfAlphaImage.fill(QColor(255, 255, 255, 128));
    ASSERT_TRUE(transparentImage.save(transparentPath, "PNG"));
    ASSERT_TRUE(opaqueImage.save(opaquePath, "PNG"));
    ASSERT_TRUE(halfAlphaImage.save(halfAlphaPath, "PNG"));
    QImage uvSetRamp(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(uvSetRamp.isNull());
    for (int y = 0; y < uvSetRamp.height(); ++y) {
        auto* row = uvSetRamp.scanLine(y);
        for (int x = 0; x < uvSetRamp.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = pixel[1] = pixel[2] = 255;
            pixel[3] = x < 4 ? 255 : 0;
        }
    }
    ASSERT_TRUE(uvSetRamp.save(uvSetRampPath, "PNG"));

    ArtifactCore::RTPbrMaterial leftMaterial;
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 0.0f;
    leftMaterial.emissionColor[0] = 1.0f;
    leftMaterial.emissionStrength = 1.0f;
    leftMaterial.alphaMode = 1;
    leftMaterial.alphaCutoff = 0.5f;
    leftMaterial.opacityTexture = ArtifactCore::UniString(transparentPath);
    ArtifactCore::RTPbrMaterial rightMaterial = leftMaterial;
    rightMaterial.emissionColor[0] = 0.0f;
    rightMaterial.emissionColor[1] = 1.0f;
    rightMaterial.opacityTexture = ArtifactCore::UniString(opaquePath);
    ArtifactCore::RTPbrMaterial backgroundMaterial;
    backgroundMaterial.baseColor[0] = backgroundMaterial.baseColor[1] =
        backgroundMaterial.baseColor[2] = 0.0f;
    backgroundMaterial.emissionColor[2] = 1.0f;
    backgroundMaterial.emissionStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-slot-background"),
        backgroundMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftCutoutPixel = pixelAt(8, 16);
    const auto rightOpaquePixel = pixelAt(24, 16);
    EXPECT_GT(leftCutoutPixel[2], leftCutoutPixel[0] + 0.5f);
    EXPECT_GT(rightOpaquePixel[1], rightOpaquePixel[2] + 0.5f);

    leftMaterial.opacityTexture = ArtifactCore::UniString(uvSetRampPath);
    rightMaterial.opacityTexture = ArtifactCore::UniString(uvSetRampPath);
    leftMaterial.opacityTextureUvSet = 0;
    rightMaterial.opacityTextureUvSet = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv0SlotOpaquePixel = pixelAt(8, 16);
    const auto uv0SlotCutoutPixel = pixelAt(24, 16);
    EXPECT_GT(uv0SlotOpaquePixel[0], uv0SlotOpaquePixel[2] + 0.5f)
        << "the left slot should sample its opaque UV0 half";
    EXPECT_GT(uv0SlotCutoutPixel[2], uv0SlotCutoutPixel[1] + 0.5f)
        << "the right slot should cut out its transparent UV0 half";

    rightMaterial.opacityTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1SlotOpaquePixel = pixelAt(24, 16);
    EXPECT_GT(uv1SlotOpaquePixel[1], uv1SlotOpaquePixel[2] + 0.5f)
        << "the right slot should independently sample its mirrored UV1 half";

    rightMaterial.opacityTextureUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto offsetUv1SlotCutoutPixel = pixelAt(24, 16);
    const auto unchangedUv0SlotOpaquePixel = pixelAt(8, 16);
    EXPECT_GT(offsetUv1SlotCutoutPixel[2], offsetUv1SlotCutoutPixel[1] + 0.5f)
        << "right-slot opacity U offset should move its sample into the cutout";
    EXPECT_GT(unchangedUv0SlotOpaquePixel[0],
              unchangedUv0SlotOpaquePixel[2] + 0.5f)
        << "right-slot opacity U offset should preserve the left slot";

    rightMaterial.opacityTextureUvOffsetU = 0.0f;
    rightMaterial.opacityTextureUvScale = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scaledUv1SlotCutoutPixel = pixelAt(24, 16);
    const auto unchangedScaledUv0SlotOpaquePixel = pixelAt(8, 16);
    EXPECT_GT(scaledUv1SlotCutoutPixel[2], scaledUv1SlotCutoutPixel[1] + 0.5f)
        << "right-slot opacity UV scale should move its sample into the cutout";
    EXPECT_GT(unchangedScaledUv0SlotOpaquePixel[0],
              unchangedScaledUv0SlotOpaquePixel[2] + 0.5f)
        << "right-slot opacity UV scale should preserve the left slot";
    rightMaterial.opacityTextureUvScale = 1.0f;

    leftMaterial.opacityTexture = ArtifactCore::UniString(opaquePath);
    rightMaterial.opacityTexture = ArtifactCore::UniString(transparentPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftOpaquePixel = pixelAt(8, 16);
    const auto rightCutoutPixel = pixelAt(24, 16);
    EXPECT_GT(leftOpaquePixel[0], leftOpaquePixel[2] + 0.5f);
    EXPECT_GT(rightCutoutPixel[2], rightCutoutPixel[1] + 0.5f);

    leftMaterial.opacityTexture = ArtifactCore::UniString(halfAlphaPath);
    leftMaterial.alphaCutoff = 0.4f;
    rightMaterial.opacityTexture = ArtifactCore::UniString(halfAlphaPath);
    rightMaterial.alphaCutoff = 0.6f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftBelowCutoffPixel = pixelAt(8, 16);
    const auto rightAboveCutoffPixel = pixelAt(24, 16);
    EXPECT_GT(leftBelowCutoffPixel[0], leftBelowCutoffPixel[2] + 0.5f)
        << "the left slot's lower cutoff should retain a half-alpha texel";
    EXPECT_GT(rightAboveCutoffPixel[2], rightAboveCutoffPixel[1] + 0.5f)
        << "the right slot's higher cutoff should cut out that same texel";

    leftMaterial.alphaCutoff = 0.6f;
    rightMaterial.alphaCutoff = 0.4f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftAboveCutoffPixel = pixelAt(8, 16);
    const auto rightBelowCutoffPixel = pixelAt(24, 16);
    EXPECT_GT(leftAboveCutoffPixel[2], leftAboveCutoffPixel[0] + 0.5f)
        << "the left slot's higher cutoff should cut out the half-alpha texel";
    EXPECT_GT(rightBelowCutoffPixel[1], rightBelowCutoffPixel[2] + 0.5f)
        << "the right slot's lower cutoff should retain that same texel";

    leftMaterial.alphaCutoff = rightMaterial.alphaCutoff = 0.5f;
    leftMaterial.opacityTexture = ArtifactCore::UniString();
    rightMaterial.opacityTexture = ArtifactCore::UniString();
    leftMaterial.opacity = 0.4f;
    rightMaterial.opacity = 0.6f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftScalarCutoutPixel = pixelAt(8, 16);
    const auto rightScalarOpaquePixel = pixelAt(24, 16);
    EXPECT_GT(leftScalarCutoutPixel[2], leftScalarCutoutPixel[0] + 0.5f)
        << "the left slot's lower scalar opacity should cut out its surface";
    EXPECT_GT(rightScalarOpaquePixel[1], rightScalarOpaquePixel[2] + 0.5f)
        << "the right slot's higher scalar opacity should retain its surface";

    leftMaterial.opacity = 0.6f;
    rightMaterial.opacity = 0.4f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftScalarOpaquePixel = pixelAt(8, 16);
    const auto rightScalarCutoutPixel = pixelAt(24, 16);
    EXPECT_GT(leftScalarOpaquePixel[0], leftScalarOpaquePixel[2] + 0.5f)
        << "the left slot's higher scalar opacity should retain its surface";
    EXPECT_GT(rightScalarCutoutPixel[2], rightScalarCutoutPixel[1] + 0.5f)
        << "the right slot's lower scalar opacity should cut out its surface";

    leftMaterial.opacityTexture = ArtifactCore::UniString(transparentPath);
    rightMaterial.opacityTexture = ArtifactCore::UniString(transparentPath);
    leftMaterial.opacity = rightMaterial.opacity = 1.0f;
    leftMaterial.alphaMode = 0;
    rightMaterial.alphaMode = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftOpaqueModePixel = pixelAt(8, 16);
    const auto rightMaskedModePixel = pixelAt(24, 16);
    EXPECT_GT(leftOpaqueModePixel[0], leftOpaqueModePixel[2] + 0.5f)
        << "opaque mode on slot 0 should retain its transparent-textured surface";
    EXPECT_GT(rightMaskedModePixel[2], rightMaskedModePixel[1] + 0.5f)
        << "masked mode on slot 1 should cut out its transparent texture";

    leftMaterial.alphaMode = 1;
    rightMaterial.alphaMode = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        leftMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-opacity-material-slots"),
        rightMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftMaskedModePixel = pixelAt(8, 16);
    const auto rightOpaqueModePixel = pixelAt(24, 16);
    EXPECT_GT(leftMaskedModePixel[2], leftMaskedModePixel[0] + 0.5f)
        << "masked mode on slot 0 should cut out its transparent texture";
    EXPECT_GT(rightOpaqueModePixel[1], rightOpaqueModePixel[2] + 0.5f)
        << "opaque mode on slot 1 should retain its transparent-textured surface";
}

TEST_F(RayTracingAcceptanceTest, SamplesOcclusionTextureWithInterpolatedMeshUvs) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("ao-uv-sampling", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("ao-uv-sampling"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data(), nullptr,
        mirroredUvs1.data()));

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const auto texturePath =
        temporaryDirectory.filePath(QStringLiteral("ao-uv-test.png"));
    QImage aoImage(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(aoImage.isNull());
    for (int y = 0; y < aoImage.height(); ++y) {
        auto* row = aoImage.scanLine(y);
        for (int x = 0; x < aoImage.width(); ++x) {
            auto* pixel = row + x * 4;
            const uchar red = x < 4 ? 0 : 255;
            const uchar inverse = x < 4 ? 255 : 0;
            pixel[0] = red;
            pixel[1] = inverse;
            pixel[2] = inverse;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(aoImage.save(texturePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.occlusionTexture = ArtifactCore::UniString(texturePath);
    material.occlusionStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("22-pbr-ao-uv-sampling.png"));

    const auto occludedPixel = pixelAt(8, 16);
    const auto openPixel = pixelAt(24, 16);
    EXPECT_GT(openPixel[0], occludedPixel[0] + 0.05f);

    material.occlusionTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1LeftPixel = pixelAt(8, 16);
    const auto uv1RightPixel = pixelAt(24, 16);
    EXPECT_NEAR(uv1LeftPixel[0], openPixel[0], 0.025f)
        << "occlusion UV1 mirroring should select the open texels on the left";
    EXPECT_NEAR(uv1RightPixel[0], occludedPixel[0], 0.025f)
        << "occlusion UV1 mirroring should select the occluded texels on the right";

    material.occlusionTextureUvSet = 0;
    const auto verticalAoPath = temporaryDirectory.filePath(
        QStringLiteral("ao-v-ramp.png"));
    QImage verticalAoImage(2, 8, QImage::Format_RGBA8888);
    ASSERT_FALSE(verticalAoImage.isNull());
    for (int y = 0; y < verticalAoImage.height(); ++y) {
        auto* row = verticalAoImage.scanLine(y);
        for (int x = 0; x < verticalAoImage.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = y < 4 ? 0 : 255;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(verticalAoImage.save(verticalAoPath, "PNG"));
    material.occlusionTexture = ArtifactCore::UniString(verticalAoPath);
    material.occlusionTextureUvOffsetU = 0.0f;
    material.occlusionTextureUvOffsetV = 0.0f;
    material.occlusionTextureUvScale = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto aoV0Top = pixelAt(8, 8);
    const auto aoV0Bottom = pixelAt(8, 24);
    EXPECT_GT(std::fabs(aoV0Top[0] - aoV0Bottom[0]), 0.05f)
        << "occlusion red channel should vary between vertical UV samples";

    material.occlusionTextureUvOffsetV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_NEAR(pixelAt(8, 8)[0], aoV0Bottom[0], 0.025f)
        << "occlusion V offset should mirror the top sample";
    EXPECT_NEAR(pixelAt(8, 24)[0], aoV0Top[0], 0.025f)
        << "occlusion V offset should mirror the bottom sample";

    material.occlusionTextureUvOffsetV = 0.0f;
    material.occlusionTextureUvScale = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_NEAR(pixelAt(8, 8)[0], aoV0Bottom[0], 0.025f)
        << "occlusion V scale should repeat the bottom sample on top";
    EXPECT_NEAR(pixelAt(8, 24)[0], aoV0Top[0], 0.025f)
        << "occlusion V scale should repeat the top sample on bottom";

    material.occlusionTexture = ArtifactCore::UniString(texturePath);
    material.occlusionTextureUvScale = 1.0f;
    material.occlusionTextureUvOffsetV = 0.0f;
    material.occlusionTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_NEAR(pixelAt(8, 16)[0], openPixel[0], 0.025f)
        << "occlusion 180-degree UV rotation should mirror the left sample";
    EXPECT_NEAR(pixelAt(24, 16)[0], occludedPixel[0], 0.025f)
        << "occlusion 180-degree UV rotation should mirror the right sample";
    material.occlusionTextureUvRotationDegrees = 0.0f;
    material.occlusionStrength = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto zeroStrengthPixel = pixelAt(8, 16);

    material.occlusionStrength = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfStrengthPixel = pixelAt(8, 16);
    EXPECT_GT(zeroStrengthPixel[0], halfStrengthPixel[0] + 0.005f)
        << "zero occlusion strength should restore the unoccluded response";
    EXPECT_GT(halfStrengthPixel[0], occludedPixel[0] + 0.005f)
        << "half occlusion strength should retain more response than full strength";

    const auto halfOcclusionMapPath = temporaryDirectory.filePath(
        QStringLiteral("ao-half-occlusion.png"));
    QImage halfOcclusionMap(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(halfOcclusionMap.isNull());
    halfOcclusionMap.fill(QColor(128, 128, 128, 255));
    ASSERT_TRUE(halfOcclusionMap.save(halfOcclusionMapPath, "PNG"));
    material.occlusionTexture = ArtifactCore::UniString(halfOcclusionMapPath);
    material.occlusionStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfMapPixel = pixelAt(8, 16);
    EXPECT_NEAR(halfMapPixel[0], halfStrengthPixel[0], 0.015f)
        << "occlusion strength should linearly mix the red channel with white";

    const auto occludedSlotPath = temporaryDirectory.filePath(
        QStringLiteral("ao-slot-occluded.png"));
    const auto openSlotPath = temporaryDirectory.filePath(
        QStringLiteral("ao-slot-open.png"));
    QImage occludedSlotMap(2, 2, QImage::Format_RGBA8888);
    QImage openSlotMap(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(occludedSlotMap.isNull());
    ASSERT_FALSE(openSlotMap.isNull());
    occludedSlotMap.fill(QColor(0, 0, 0, 255));
    openSlotMap.fill(QColor(255, 255, 255, 255));
    ASSERT_TRUE(occludedSlotMap.save(occludedSlotPath, "PNG"));
    ASSERT_TRUE(openSlotMap.save(openSlotPath, "PNG"));

    constexpr std::array<std::uint32_t, 2> materialIndices{0, 1};
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("ao-uv-sampling"), vertices.data(), 4,
        indices.data(), 6, nullptr, materialIndices.data(), uvs.data(), nullptr,
        mirroredUvs1.data()));
    ArtifactCore::RTPbrMaterial occludedSlotMaterial;
    occludedSlotMaterial.baseColor[0] = occludedSlotMaterial.baseColor[1] =
        occludedSlotMaterial.baseColor[2] = 0.7f;
    occludedSlotMaterial.occlusionTexture =
        ArtifactCore::UniString(occludedSlotPath);
    occludedSlotMaterial.occlusionStrength = 1.0f;
    ArtifactCore::RTPbrMaterial openSlotMaterial = occludedSlotMaterial;
    openSlotMaterial.occlusionTexture = ArtifactCore::UniString(openSlotPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), occludedSlotMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), openSlotMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto openLeftSlotPixel = pixelAt(8, 16);
    const auto occludedRightSlotPixel = pixelAt(24, 16);

    occludedSlotMaterial.occlusionTexture = ArtifactCore::UniString(openSlotPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), occludedSlotMaterial, 0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedOpenLeftSlotPixel = pixelAt(8, 16);
    const auto openRightSlotPixel = pixelAt(24, 16);
    EXPECT_GT(openRightSlotPixel[0], occludedRightSlotPixel[0] + 0.005f)
        << "slot 0 occlusion map should change its shading response";
    EXPECT_NEAR(unchangedOpenLeftSlotPixel[0], openLeftSlotPixel[0], 0.025f)
        << "changing slot 0 occlusion map should preserve slot 1";

    occludedSlotMaterial.occlusionTexture =
        ArtifactCore::UniString(occludedSlotPath);
    openSlotMaterial.occlusionTexture =
        ArtifactCore::UniString(occludedSlotPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), occludedSlotMaterial, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("ao-uv-sampling"), openSlotMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto occludedLeftSlotPixel = pixelAt(8, 16);
    const auto unchangedOpenRightSlotPixel = pixelAt(24, 16);
    EXPECT_GT(openLeftSlotPixel[0], occludedLeftSlotPixel[0] + 0.005f)
        << "slot 1 occlusion map should change its shading response";
    EXPECT_NEAR(unchangedOpenRightSlotPixel[0], openRightSlotPixel[0], 0.025f)
        << "changing slot 1 occlusion map should preserve slot 0";
}

TEST_F(RayTracingAcceptanceTest, InterpolatesSrgbVertexColorsAcrossTriangles) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 16> vertexColors{
        1.0f, 0.05f, 0.05f, 1.0f, 0.05f, 1.0f, 0.05f, 1.0f,
        0.05f, 1.0f, 0.05f, 1.0f, 1.0f, 0.05f, 0.05f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-srgb-vertex-colors", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-srgb-vertex-colors"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, nullptr, vertexColors.data()));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.8f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-srgb-vertex-colors"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));

    const auto leftPixel = pixelAt(8, 16);
    const auto rightPixel = pixelAt(24, 16);
    EXPECT_GT(leftPixel[0], leftPixel[1] + 0.01f)
        << "the interpolated left-side vertex color should remain red-dominant";
    EXPECT_GT(rightPixel[1], rightPixel[0] + 0.01f)
        << "the interpolated right-side vertex color should remain green-dominant";
}

TEST_F(RayTracingAcceptanceTest,
       SelectsMipLevelsForMinifiedOcclusionTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-occlusion-mip", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-occlusion-mip"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("occlusion-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("occlusion-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_Grayscale8);
    QImage average(textureSize, textureSize, QImage::Format_Grayscale8);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            checkerRow[x] = ((x + y) & 1) != 0 ? 255 : 0;
            averageRow[x] = 127;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.occlusionStrength = 1.0f;
    material.occlusionTexture = ArtifactCore::UniString(checkerPath);
    const auto traceAndSample = [&](const ArtifactCore::RTPbrMaterial& input,
                                    const int x) {
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-occlusion-mip"), input));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(x, 16)[0];
    };
    std::array<float, 5> checkerSamples{};
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }

    material.occlusionTexture = ArtifactCore::UniString(averagePath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)],
                    traceAndSample(material, 14 + index), 0.025f)
            << "minified occlusion sample " << index
            << " should match its linear mip average";
    }
}

TEST_F(RayTracingAcceptanceTest, SamplesBaseColorAndMetallicRoughnessMaps) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-color-surface-maps", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-color-surface-maps"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data()));

    QTemporaryDir surfaceMapDirectory;
    ASSERT_TRUE(surfaceMapDirectory.isValid());
    const auto baseColorPath = std::filesystem::path(
        surfaceMapDirectory.filePath(QStringLiteral("base-color.png"))
            .toStdString());
    const auto metallicMapPath = std::filesystem::path(
        surfaceMapDirectory.filePath(QStringLiteral("metallic.png"))
            .toStdString());
    const auto dielectricMapPath = std::filesystem::path(
        surfaceMapDirectory.filePath(QStringLiteral("dielectric.png"))
            .toStdString());
    QImage baseColorImage(8, 2, QImage::Format_RGBA8888);
    QImage metallicImage(8, 2, QImage::Format_RGBA8888);
    QImage dielectricImage(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(baseColorImage.isNull());
    ASSERT_FALSE(metallicImage.isNull());
    ASSERT_FALSE(dielectricImage.isNull());
    for (int y = 0; y < 2; ++y) {
        auto* colorRow = baseColorImage.scanLine(y);
        auto* metallicRow = metallicImage.scanLine(y);
        auto* dielectricRow = dielectricImage.scanLine(y);
        for (int x = 0; x < 8; ++x) {
            const bool right = x >= 4;
            auto* color = colorRow + x * 4;
            color[0] = right ? 0 : 255;
            color[1] = 0;
            color[2] = right ? 255 : 0;
            color[3] = 255;
            auto* metallic = metallicRow + x * 4;
            metallic[0] = 255;
            metallic[1] = 255;
            metallic[2] = right ? 255 : 0;
            metallic[3] = 255;
            auto* dielectric = dielectricRow + x * 4;
            dielectric[0] = 255;
            dielectric[1] = 255;
            dielectric[2] = 0;
            dielectric[3] = 255;
        }
    }
    ASSERT_TRUE(baseColorImage.save(QString::fromStdString(baseColorPath.string()), "PNG"));
    ASSERT_TRUE(metallicImage.save(QString::fromStdString(metallicMapPath.string()), "PNG"));
    ASSERT_TRUE(dielectricImage.save(QString::fromStdString(dielectricMapPath.string()), "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 1.0f;
    material.metallic = 1.0f;
    material.roughness = 0.6f;
    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(baseColorPath.string()));
    material.metallicRoughnessTexture = ArtifactCore::UniString(
        QString::fromStdString(metallicMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("23-pbr-basecolor-metallic-roughness.png"));
    const auto mappedMetallicLeft = pixelAt(8, 16);
    const auto mappedMetallicRight = pixelAt(24, 16);
    EXPECT_GT(mappedMetallicLeft[0], mappedMetallicLeft[2] + 0.1f);
    EXPECT_GT(mappedMetallicRight[2], mappedMetallicRight[0] + 0.1f);

    material.metallicRoughnessTexture = ArtifactCore::UniString(
        QString::fromStdString(dielectricMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baseColorUv0Left = pixelAt(8, 16);
    const auto baseColorUv0Right = pixelAt(24, 16);
    EXPECT_GT(baseColorUv0Left[0], baseColorUv0Left[2] + 0.1f);
    EXPECT_GT(baseColorUv0Right[2], baseColorUv0Right[0] + 0.1f);

    QImage transparentBaseColorImage = baseColorImage;
    for (int y = 0; y < transparentBaseColorImage.height(); ++y) {
        auto* row = transparentBaseColorImage.scanLine(y);
        for (int x = 0; x < transparentBaseColorImage.width(); ++x) {
            row[x * 4 + 3] = 0;
        }
    }
    const auto transparentBaseColorPath = std::filesystem::path(
        surfaceMapDirectory.filePath(
            QStringLiteral("base-color-zero-alpha.png")).toStdString());
    ASSERT_TRUE(transparentBaseColorImage.save(
        QString::fromStdString(transparentBaseColorPath.string()), "PNG"));
    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(transparentBaseColorPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto zeroAlphaBaseColorLeft = pixelAt(8, 16);
    const auto zeroAlphaBaseColorRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(zeroAlphaBaseColorLeft[channel], baseColorUv0Left[channel],
                    0.02f)
            << "opaque base-color shading should ignore texture alpha, left "
            << "channel " << channel;
        EXPECT_NEAR(zeroAlphaBaseColorRight[channel], baseColorUv0Right[channel],
                    0.02f)
            << "opaque base-color shading should ignore texture alpha, right "
            << "channel " << channel;
    }

    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(baseColorPath.string()));
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfFactorBaseColorLeft = pixelAt(8, 16);
    const auto halfFactorBaseColorRight = pixelAt(24, 16);

    QImage halfLinearBaseColorImage = baseColorImage;
    for (int y = 0; y < halfLinearBaseColorImage.height(); ++y) {
        auto* row = halfLinearBaseColorImage.scanLine(y);
        for (int x = 0; x < halfLinearBaseColorImage.width(); ++x) {
            auto* pixel = row + x * 4;
            const bool right = x >= halfLinearBaseColorImage.width() / 2;
            pixel[0] = right ? 0 : 188;
            pixel[2] = right ? 188 : 0;
        }
    }
    const auto halfLinearBaseColorPath = std::filesystem::path(
        surfaceMapDirectory.filePath(
            QStringLiteral("base-color-half-linear.png")).toStdString());
    ASSERT_TRUE(halfLinearBaseColorImage.save(
        QString::fromStdString(halfLinearBaseColorPath.string()), "PNG"));
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 1.0f;
    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(halfLinearBaseColorPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfLinearTextureLeft = pixelAt(8, 16);
    const auto halfLinearTextureRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(halfLinearTextureLeft[channel],
                    halfFactorBaseColorLeft[channel], 0.025f)
            << "base-color factor should multiply the decoded texture, left "
            << "channel " << channel;
        EXPECT_NEAR(halfLinearTextureRight[channel],
                    halfFactorBaseColorRight[channel], 0.025f)
            << "base-color factor should multiply the decoded texture, right "
            << "channel " << channel;
    }

    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(baseColorPath.string()));
    material.baseColorUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baseColorOffsetLeft = pixelAt(8, 16);
    const auto baseColorOffsetRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(baseColorOffsetLeft[channel], baseColorUv0Right[channel],
                    0.035f)
            << "base-color U offset should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(baseColorOffsetRight[channel], baseColorUv0Left[channel],
                    0.035f)
            << "base-color U offset should mirror the right sample, channel "
            << channel;
    }

    material.baseColorUvOffsetU = 0.0f;
    material.baseColorUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baseColorRotationLeft = pixelAt(8, 16);
    const auto baseColorRotationRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(baseColorRotationLeft[channel], baseColorUv0Right[channel],
                    0.035f)
            << "base-color 180-degree UV rotation should mirror the left "
               "sample, channel "
            << channel;
        EXPECT_NEAR(baseColorRotationRight[channel], baseColorUv0Left[channel],
                    0.035f)
            << "base-color 180-degree UV rotation should mirror the right "
               "sample, channel "
            << channel;
    }
    material.baseColorUvRotationDegrees = 0.0f;
    const auto verticalBaseColorPath = surfaceMapDirectory.filePath(
        QStringLiteral("base-color-v-ramp.png"));
    QImage verticalBaseColorImage(2, 8, QImage::Format_RGBA8888);
    ASSERT_FALSE(verticalBaseColorImage.isNull());
    for (int y = 0; y < verticalBaseColorImage.height(); ++y) {
        auto* row = verticalBaseColorImage.scanLine(y);
        for (int x = 0; x < verticalBaseColorImage.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = y < 4 ? 255 : 0;
            pixel[1] = 0;
            pixel[2] = y < 4 ? 0 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(verticalBaseColorImage.save(verticalBaseColorPath, "PNG"));
    material.baseColorTexture = ArtifactCore::UniString(verticalBaseColorPath);
    material.baseColorUvOffsetV = 0.0f;
    material.baseColorUvScale = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baseColorV0Top = pixelAt(12, 8);
    const auto baseColorV0Bottom = pixelAt(12, 24);
    const float baseColorV0Difference = std::max(
        std::fabs(baseColorV0Top[0] - baseColorV0Bottom[0]),
        std::max(std::fabs(baseColorV0Top[1] - baseColorV0Bottom[1]),
                 std::fabs(baseColorV0Top[2] - baseColorV0Bottom[2])));
    EXPECT_GT(baseColorV0Difference, 0.1f)
        << "vertical base-color map should produce distinct top and bottom samples";

    material.baseColorUvOffsetV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baseColorVOffsetTop = pixelAt(12, 8);
    const auto baseColorVOffsetBottom = pixelAt(12, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(baseColorVOffsetTop[channel], baseColorV0Bottom[channel],
                    0.04f)
            << "base-color V offset should mirror the top sample, channel "
            << channel;
        EXPECT_NEAR(baseColorVOffsetBottom[channel], baseColorV0Top[channel],
                    0.04f)
            << "base-color V offset should mirror the bottom sample, channel "
            << channel;
    }

    material.baseColorUvOffsetV = 0.0f;
    material.baseColorUvScale = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baseColorVScaleTop = pixelAt(12, 8);
    const auto baseColorVScaleBottom = pixelAt(12, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(baseColorVScaleTop[channel], baseColorV0Bottom[channel],
                    0.04f)
            << "base-color V scale should repeat the bottom sample on top, channel "
            << channel;
        EXPECT_NEAR(baseColorVScaleBottom[channel], baseColorV0Top[channel],
                    0.04f)
            << "base-color V scale should repeat the top sample on bottom, channel "
            << channel;
    }

    material.baseColorUvScale = 1.0f;
    material.baseColorUvOffsetV = 0.0f;
    material.baseColorTexture = ArtifactCore::UniString{};
    material.metallicRoughnessTexture = ArtifactCore::UniString(
        QString::fromStdString(metallicMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedMapUv0Left = pixelAt(8, 16);
    const auto packedMapUv0Right = pixelAt(24, 16);
    EXPECT_GT(packedMapUv0Left[0], packedMapUv0Left[2] + 0.1f);
    EXPECT_GT(packedMapUv0Right[2], packedMapUv0Right[0] + 0.1f);

    material.metallicRoughnessUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedMapOffsetLeft = pixelAt(8, 16);
    const auto packedMapOffsetRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(packedMapOffsetLeft[channel], packedMapUv0Right[channel],
                    0.035f)
            << "packed metallic-roughness U offset should mirror the left "
               "sample, channel " << channel;
        EXPECT_NEAR(packedMapOffsetRight[channel], packedMapUv0Left[channel],
                    0.035f)
            << "packed metallic-roughness U offset should mirror the right "
               "sample, channel " << channel;
    }

    const auto verticalPackedMapPath = surfaceMapDirectory.filePath(
        QStringLiteral("packed-metallic-roughness-v-ramp.png"));
    QImage verticalPackedMap(2, 8, QImage::Format_RGBA8888);
    ASSERT_FALSE(verticalPackedMap.isNull());
    for (int y = 0; y < verticalPackedMap.height(); ++y) {
        auto* row = verticalPackedMap.scanLine(y);
        for (int x = 0; x < verticalPackedMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 255;
            pixel[1] = y < 4 ? 0 : 255;
            pixel[2] = 128;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(verticalPackedMap.save(verticalPackedMapPath, "PNG"));
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.baseColorTexture = ArtifactCore::UniString{};
    material.metallic = 0.0f;
    material.roughness = 1.0f;
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(verticalPackedMapPath);
    material.metallicRoughnessUvOffsetU = 0.0f;
    material.metallicRoughnessUvOffsetV = 0.0f;
    material.metallicRoughnessUvScale = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedV0Top = pixelAt(12, 8);
    const auto packedV0Bottom = pixelAt(12, 24);
    const float packedV0Difference = std::max(
        std::fabs(packedV0Top[0] - packedV0Bottom[0]),
        std::max(std::fabs(packedV0Top[1] - packedV0Bottom[1]),
                 std::fabs(packedV0Top[2] - packedV0Bottom[2])));
    EXPECT_GT(packedV0Difference, 0.02f)
        << "packed G roughness should vary between vertical UV samples";

    material.metallicRoughnessUvOffsetV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedVOffsetTop = pixelAt(12, 8);
    const auto packedVOffsetBottom = pixelAt(12, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(packedVOffsetTop[channel], packedV0Bottom[channel], 0.04f)
            << "packed metallic-roughness V offset should mirror the top "
               "sample, channel "
            << channel;
        EXPECT_NEAR(packedVOffsetBottom[channel], packedV0Top[channel], 0.04f)
            << "packed metallic-roughness V offset should mirror the bottom "
               "sample, channel "
            << channel;
    }

    material.metallicRoughnessUvOffsetV = 0.0f;
    material.metallicRoughnessUvScale = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedVScaleTop = pixelAt(12, 8);
    const auto packedVScaleBottom = pixelAt(12, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(packedVScaleTop[channel], packedV0Bottom[channel], 0.04f)
            << "packed metallic-roughness V scale should repeat the bottom "
               "sample on top, channel "
            << channel;
        EXPECT_NEAR(packedVScaleBottom[channel], packedV0Top[channel], 0.04f)
            << "packed metallic-roughness V scale should repeat the top "
               "sample on bottom, channel "
            << channel;
    }

    material.metallicRoughnessUvOffsetU = 0.0f;
    material.metallicRoughnessUvOffsetV = 0.0f;
    material.metallicRoughnessUvScale = 1.0f;
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(metallicMapPath.string());
    material.metallic = 1.0f;
    material.roughness = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedMetallicBChannelLeft = pixelAt(8, 16);
    const auto packedMetallicBChannelRight = pixelAt(24, 16);

    QImage packedMetallicOtherChannelsChanged = metallicImage;
    for (int y = 0; y < packedMetallicOtherChannelsChanged.height(); ++y) {
        auto* row = packedMetallicOtherChannelsChanged.scanLine(y);
        for (int x = 0; x < packedMetallicOtherChannelsChanged.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 0;
            pixel[1] = 0;
            pixel[3] = 0;
        }
    }
    const auto packedMetallicOtherChannelsPath = surfaceMapDirectory.filePath(
        QStringLiteral("packed-metallic-other-channels.png"));
    ASSERT_TRUE(packedMetallicOtherChannelsChanged.save(
        packedMetallicOtherChannelsPath, "PNG"));
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(packedMetallicOtherChannelsPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedMetallicOtherChannelsLeft = pixelAt(8, 16);
    const auto packedMetallicOtherChannelsRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(packedMetallicOtherChannelsLeft[channel],
                    packedMetallicBChannelLeft[channel], 0.02f)
            << "packed metallic should read blue independently, left channel "
            << channel;
        EXPECT_NEAR(packedMetallicOtherChannelsRight[channel],
                    packedMetallicBChannelRight[channel], 0.02f)
            << "packed metallic should read blue independently, right channel "
            << channel;
    }

    const auto greenRoughnessMapPath = surfaceMapDirectory.filePath(
        QStringLiteral("green-roughness.png"));
    QImage greenRoughnessMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(greenRoughnessMap.isNull());
    for (int y = 0; y < greenRoughnessMap.height(); ++y) {
        auto* row = greenRoughnessMap.scanLine(y);
        for (int x = 0; x < greenRoughnessMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 255;
            pixel[1] = x < greenRoughnessMap.width() / 2 ? 0 : 255;
            pixel[2] = 0;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(greenRoughnessMap.save(greenRoughnessMapPath, "PNG"));
    material.baseColorTexture = ArtifactCore::UniString{};
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.metallic = 0.0f;
    material.roughness = 1.0f;
    material.metallicRoughnessUvOffsetU = 0.0f;
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(greenRoughnessMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedRoughnessLeft = pixelAt(8, 16);
    const auto packedRoughnessRight = pixelAt(24, 16);
    const float packedRoughnessDifference = std::max(
        std::fabs(packedRoughnessLeft[0] - packedRoughnessRight[0]),
        std::max(std::fabs(packedRoughnessLeft[1] - packedRoughnessRight[1]),
                 std::fabs(packedRoughnessLeft[2] - packedRoughnessRight[2])));
    EXPECT_GT(packedRoughnessDifference, 0.02f)
        << "packed metallic-roughness green should vary roughness while red "
           "and blue remain constant";

    QImage packedRoughnessOtherChannelsChanged = greenRoughnessMap;
    for (int y = 0; y < packedRoughnessOtherChannelsChanged.height(); ++y) {
        auto* row = packedRoughnessOtherChannelsChanged.scanLine(y);
        for (int x = 0; x < packedRoughnessOtherChannelsChanged.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 0;
            pixel[2] = 255;
        }
    }
    const auto packedRoughnessOtherChannelsPath =
        surfaceMapDirectory.filePath(
            QStringLiteral("packed-roughness-other-channels.png"));
    ASSERT_TRUE(packedRoughnessOtherChannelsChanged.save(
        packedRoughnessOtherChannelsPath, "PNG"));
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(packedRoughnessOtherChannelsPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto packedOtherChannelsLeft = pixelAt(8, 16);
    const auto packedOtherChannelsRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(packedOtherChannelsLeft[channel], packedRoughnessLeft[channel],
                    0.02f)
            << "packed roughness should read green independently, left channel "
            << channel;
        EXPECT_NEAR(packedOtherChannelsRight[channel],
                    packedRoughnessRight[channel], 0.02f)
            << "packed roughness should read green independently, right channel "
            << channel;
    }

    QImage transparentPackedMap = greenRoughnessMap;
    for (int y = 0; y < transparentPackedMap.height(); ++y) {
        auto* row = transparentPackedMap.scanLine(y);
        for (int x = 0; x < transparentPackedMap.width(); ++x) {
            row[x * 4 + 3] = 0;
        }
    }
    const auto transparentPackedMapPath = surfaceMapDirectory.filePath(
        QStringLiteral("transparent-packed-map.png"));
    ASSERT_TRUE(transparentPackedMap.save(
        transparentPackedMapPath, "PNG"));
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(transparentPackedMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto transparentPackedLeft = pixelAt(8, 16);
    const auto transparentPackedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(transparentPackedLeft[channel], packedRoughnessLeft[channel],
                    0.01f)
            << "packed metallic-roughness alpha should not change RGB input, "
               "left channel " << channel;
        EXPECT_NEAR(transparentPackedRight[channel],
                    packedRoughnessRight[channel], 0.01f)
            << "packed metallic-roughness alpha should not change RGB input, "
               "right channel " << channel;
    }
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(greenRoughnessMapPath);

    material.roughness = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfFactorPackedRoughness = pixelAt(24, 16);

    QImage halfGreenRoughnessMap = greenRoughnessMap;
    for (int y = 0; y < halfGreenRoughnessMap.height(); ++y) {
        auto* row = halfGreenRoughnessMap.scanLine(y);
        for (int x = halfGreenRoughnessMap.width() / 2;
             x < halfGreenRoughnessMap.width(); ++x) {
            row[x * 4 + 1] = 128;
        }
    }
    const auto halfGreenRoughnessMapPath = surfaceMapDirectory.filePath(
        QStringLiteral("half-green-roughness.png"));
    ASSERT_TRUE(halfGreenRoughnessMap.save(
        halfGreenRoughnessMapPath, "PNG"));
    material.roughness = 1.0f;
    material.metallicRoughnessTexture =
        ArtifactCore::UniString(halfGreenRoughnessMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfTexelPackedRoughness = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(halfTexelPackedRoughness[channel],
                    halfFactorPackedRoughness[channel], 0.035f)
            << "packed G roughness factor should multiply sampled roughness, "
               "channel " << channel;
    }

    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 1.0f;
    material.metallic = 1.0f;
    material.roughness = 0.6f;
    material.metallicRoughnessUvOffsetU = 0.0f;
    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(baseColorPath.string()));
    material.metallicRoughnessTexture = ArtifactCore::UniString(
        QString::fromStdString(dielectricMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto dielectricRight = pixelAt(24, 16);
    const float rightMapDifference = std::max(
        std::fabs(dielectricRight[0] - mappedMetallicRight[0]),
        std::max(std::fabs(dielectricRight[1] - mappedMetallicRight[1]),
                 std::fabs(dielectricRight[2] - mappedMetallicRight[2])));
    EXPECT_GT(rightMapDifference, 0.01f);

    QTemporaryDir linearColorDirectory;
    ASSERT_TRUE(linearColorDirectory.isValid());
    const QString linearExrPath = linearColorDirectory.filePath(
        QStringLiteral("linear-base-color.exr"));
    const QString srgbReferencePath = linearColorDirectory.filePath(
        QStringLiteral("srgb-base-color.png"));
    constexpr std::array<float, 16> linearBaseColorPixels{
        0.25f, 0.5f, 0.75f, 1.0f,
        0.25f, 0.5f, 0.75f, 1.0f,
        0.25f, 0.5f, 0.75f, 1.0f,
        0.25f, 0.5f, 0.75f, 1.0f};
    ArtifactCore::OpenExr exr;
    ASSERT_TRUE(exr.writeRGBA32F(linearExrPath, linearBaseColorPixels.data(),
                                 2, 2));
    QImage srgbReference(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(srgbReference.isNull());
    srgbReference.fill(QColor(137, 188, 225, 255));
    ASSERT_TRUE(srgbReference.save(srgbReferencePath, "PNG"));

    material.baseColorTexture = ArtifactCore::UniString(srgbReferencePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto srgbReferencePixel = pixelAt(16, 16);

    material.baseColorTexture = ArtifactCore::UniString(linearExrPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto linearExrPixel = pixelAt(16, 16);
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(linearExrPixel[channel], srgbReferencePixel[channel], 0.02f)
            << "linear float base-color channel " << channel
            << " should match its sRGB-encoded PNG reference";
    }

    material.baseColorTexture = ArtifactCore::UniString{};
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto clearedBaseColorMapPixel = pixelAt(16, 16);
    const float removedMapDifference = std::max(
        std::fabs(clearedBaseColorMapPixel[0] - linearExrPixel[0]),
        std::max(std::fabs(clearedBaseColorMapPixel[1] - linearExrPixel[1]),
                 std::fabs(clearedBaseColorMapPixel[2] - linearExrPixel[2])));
    EXPECT_GT(removedMapDifference, 0.02f)
        << "clearing the base-color map should restore the scalar material color";

    QTemporaryDir missingTextureDirectory;
    ASSERT_TRUE(missingTextureDirectory.isValid());
    material.baseColorTexture = ArtifactCore::UniString(
        missingTextureDirectory.filePath(QStringLiteral("missing-base.png")));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto missingBaseColorPixel = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(missingBaseColorPixel[channel],
                    clearedBaseColorMapPixel[channel], 0.01f)
            << "a missing base-color map should preserve scalar color, channel "
            << channel;
    }

    material.baseColorTexture = ArtifactCore::UniString();
    material.metallicRoughnessTexture = ArtifactCore::UniString();
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scalarSurfacePixel = pixelAt(16, 16);

    material.metallicRoughnessTexture = ArtifactCore::UniString(
        missingTextureDirectory.filePath(
            QStringLiteral("missing-metallic-roughness.png")));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-color-surface-maps"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto missingSurfacePixel = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(missingSurfacePixel[channel], scalarSurfacePixel[channel],
                    0.01f)
            << "a missing metallic-roughness map should preserve scalar "
               "factors, channel " << channel;
    }

}

TEST_F(RayTracingAcceptanceTest, SelectsMetallicAndRoughnessTextureUvSetsTogether) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-metallic-roughness-uv-sets", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-metallic-roughness-uv-sets"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data(),
        nullptr, mirroredUvs1.data()));

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    const QString metallicMapPath = textureDirectory.filePath(
        QStringLiteral("combined-metallic.png"));
    const QString roughnessMapPath = textureDirectory.filePath(
        QStringLiteral("combined-roughness.png"));
    auto makeHorizontalRamp = [](const QString& path) {
        QImage map(8, 2, QImage::Format_RGBA8888);
        if (map.isNull()) {
            return false;
        }
        for (int y = 0; y < map.height(); ++y) {
            auto* row = map.scanLine(y);
            for (int x = 0; x < map.width(); ++x) {
                auto* pixel = row + x * 4;
                pixel[0] = x < 4 ? 0 : 255;
                pixel[1] = pixel[2] = pixel[3] = 255;
            }
        }
        return map.save(path, "PNG");
    };
    ASSERT_TRUE(makeHorizontalRamp(metallicMapPath));
    ASSERT_TRUE(makeHorizontalRamp(roughnessMapPath));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.metallic = 1.0f;
    material.roughness = 1.0f;
    material.metallicTexture = ArtifactCore::UniString(metallicMapPath);
    material.roughnessTexture = ArtifactCore::UniString(roughnessMapPath);
    const ArtifactCore::UniString geometryName(
        "pbr-metallic-roughness-uv-sets");
    ASSERT_TRUE(manager_->setInstancePbrMaterial(geometryName, material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto bothUv0Left = pixelAt(8, 16);
    const auto bothUv0Right = pixelAt(24, 16);
    EXPECT_GT(std::max(std::fabs(bothUv0Left[0] - bothUv0Right[0]),
                       std::max(std::fabs(bothUv0Left[1] - bothUv0Right[1]),
                                std::fabs(bothUv0Left[2] - bothUv0Right[2]))),
              0.02f)
        << "both independent maps should contribute while using UV0";

    material.metallicTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(geometryName, material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto metallicUv1Left = pixelAt(8, 16);
    const auto metallicUv1Right = pixelAt(24, 16);
    float metallicRoutingDifference = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        metallicRoutingDifference = std::max(
            metallicRoutingDifference,
            std::fabs(metallicUv1Left[channel] - bothUv0Left[channel]));
        metallicRoutingDifference = std::max(
            metallicRoutingDifference,
            std::fabs(metallicUv1Right[channel] - bothUv0Right[channel]));
    }
    EXPECT_GT(metallicRoutingDifference, 0.02f)
        << "switching metallic to UV1 should affect output with roughness still on UV0";

    material.metallicTextureUvSet = 0;
    material.roughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(geometryName, material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughnessUv1Left = pixelAt(8, 16);
    const auto roughnessUv1Right = pixelAt(24, 16);
    float roughnessRoutingDifference = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        roughnessRoutingDifference = std::max(
            roughnessRoutingDifference,
            std::fabs(roughnessUv1Left[channel] - bothUv0Left[channel]));
        roughnessRoutingDifference = std::max(
            roughnessRoutingDifference,
            std::fabs(roughnessUv1Right[channel] - bothUv0Right[channel]));
    }
    EXPECT_GT(roughnessRoutingDifference, 0.02f)
        << "switching roughness to UV1 should affect output with metallic still on UV0";
}

TEST_F(RayTracingAcceptanceTest, SelectsIndependentMetallicTextureUvSet) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-metallic-uv-set", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-metallic-uv-set"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data(), nullptr,
        mirroredUvs1.data()));

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    const QString metallicMapPath = textureDirectory.filePath(
        QStringLiteral("independent-metallic.png"));
    QImage metallicMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(metallicMap.isNull());
    for (int y = 0; y < metallicMap.height(); ++y) {
        auto* row = metallicMap.scanLine(y);
        for (int x = 0; x < metallicMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 0 : 255;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(metallicMap.save(metallicMapPath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.metallic = 1.0f;
    material.roughness = 0.4f;
    material.metallicTexture = ArtifactCore::UniString(metallicMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv0Left = pixelAt(8, 16);
    const auto uv0Right = pixelAt(24, 16);
    const float uv0Difference = std::max(
        std::fabs(uv0Left[0] - uv0Right[0]),
        std::max(std::fabs(uv0Left[1] - uv0Right[1]),
                 std::fabs(uv0Left[2] - uv0Right[2])));
    EXPECT_GT(uv0Difference, 0.02f)
        << "the metallic map's red channel should distinguish the two UV0 halves";

    QImage metallicOtherChannelsChanged = metallicMap;
    for (int y = 0; y < metallicOtherChannelsChanged.height(); ++y) {
        auto* row = metallicOtherChannelsChanged.scanLine(y);
        for (int x = 0; x < metallicOtherChannelsChanged.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[1] = 0;
            pixel[2] = 0;
            pixel[3] = 0;
        }
    }
    const QString metallicOtherChannelsPath = textureDirectory.filePath(
        QStringLiteral("independent-metallic-other-channels.png"));
    ASSERT_TRUE(metallicOtherChannelsChanged.save(
        metallicOtherChannelsPath, "PNG"));
    material.metallicTexture =
        ArtifactCore::UniString(metallicOtherChannelsPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto otherChannelsChangedLeft = pixelAt(8, 16);
    const auto otherChannelsChangedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(otherChannelsChangedLeft[channel], uv0Left[channel], 0.02f)
            << "independent metallic should read red only, left channel "
            << channel;
        EXPECT_NEAR(otherChannelsChangedRight[channel], uv0Right[channel],
                    0.02f)
            << "independent metallic should read red only, right channel "
            << channel;
    }
    material.metallicTexture = ArtifactCore::UniString(metallicMapPath);

    const QString halfMetallicMapPath = textureDirectory.filePath(
        QStringLiteral("independent-metallic-half.png"));
    QImage halfMetallicMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(halfMetallicMap.isNull());
    for (int y = 0; y < halfMetallicMap.height(); ++y) {
        auto* row = halfMetallicMap.scanLine(y);
        for (int x = 0; x < halfMetallicMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 0 : 128;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(halfMetallicMap.save(halfMetallicMapPath, "PNG"));

    constexpr float halfMetallicFactor = 128.0f / 255.0f;
    material.metallic = halfMetallicFactor;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto factorScaledLeft = pixelAt(8, 16);
    const auto factorScaledRight = pixelAt(24, 16);

    material.metallic = 1.0f;
    material.metallicTexture = ArtifactCore::UniString(halfMetallicMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto mapScaledLeft = pixelAt(8, 16);
    const auto mapScaledRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(factorScaledLeft[channel], mapScaledLeft[channel], 0.035f)
            << "metallic scalar should multiply the sampled red channel, left "
               "output channel " << channel;
        EXPECT_NEAR(factorScaledRight[channel], mapScaledRight[channel], 0.035f)
            << "metallic scalar and a half-valued map should produce the same "
               "effective factor, output channel " << channel;
    }

    material.metallicTexture = ArtifactCore::UniString(metallicMapPath);
    material.metallic = 1.0f;
    material.metallicTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1Left = pixelAt(8, 16);
    const auto uv1Right = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(uv1Left[channel], uv0Right[channel], 0.035f)
            << "metallic UV1 should mirror the left sample, channel " << channel;
        EXPECT_NEAR(uv1Right[channel], uv0Left[channel], 0.035f)
            << "metallic UV1 should mirror the right sample, channel " << channel;
    }

    material.metallicTextureUvSet = 0;
    material.metallicTextureUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto offsetLeft = pixelAt(8, 16);
    const auto offsetRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(offsetLeft[channel], uv0Right[channel], 0.035f)
            << "metallic-map U offset should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(offsetRight[channel], uv0Left[channel], 0.035f)
            << "metallic-map U offset should mirror the right sample, channel "
            << channel;
    }

    material.metallicTextureUvOffsetU = 0.0f;
    material.metallicTextureUvScaleU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scaledLeft = pixelAt(8, 16);
    const auto scaledRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(scaledLeft[channel], uv0Left[channel], 0.035f)
            << "metallic-map U scale should keep the left texel on the dark half";
        EXPECT_NEAR(scaledRight[channel], uv0Left[channel], 0.035f)
            << "metallic-map U scale should repeat the dark half on the right";
    }

    material.metallicTextureUvScaleU = 1.0f;
    material.metallicTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedLeft = pixelAt(8, 16);
    const auto rotatedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rotatedLeft[channel], uv0Right[channel], 0.035f)
            << "metallic-map 180-degree rotation should mirror the left sample";
        EXPECT_NEAR(rotatedRight[channel], uv0Left[channel], 0.035f)
            << "metallic-map 180-degree rotation should mirror the right sample";
    }

    const QString verticalMetallicMapPath = textureDirectory.filePath(
        QStringLiteral("independent-metallic-vertical.png"));
    QImage verticalMetallicMap(2, 8, QImage::Format_RGBA8888);
    ASSERT_FALSE(verticalMetallicMap.isNull());
    for (int y = 0; y < verticalMetallicMap.height(); ++y) {
        auto* row = verticalMetallicMap.scanLine(y);
        const uchar red = y < 4 ? 0 : 255;
        for (int x = 0; x < verticalMetallicMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = red;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(verticalMetallicMap.save(verticalMetallicMapPath, "PNG"));
    material.metallicTexture = ArtifactCore::UniString(verticalMetallicMapPath);
    material.metallicTextureUvRotationDegrees = 0.0f;
    material.metallicTextureUvScaleV = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uvVTop = pixelAt(16, 8);
    const auto uvVBottom = pixelAt(16, 24);
    EXPECT_GT(std::fabs(uvVTop[0] - uvVBottom[0]), 0.02f)
        << "the vertical metallic map should distinguish its V samples";

    material.metallicTextureUvOffsetV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto offsetVTop = pixelAt(16, 8);
    const auto offsetVBottom = pixelAt(16, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(offsetVTop[channel], uvVBottom[channel], 0.035f)
            << "metallic-map V offset should mirror the top sample";
        EXPECT_NEAR(offsetVBottom[channel], uvVTop[channel], 0.035f)
            << "metallic-map V offset should mirror the bottom sample";
    }

    material.metallicTextureUvOffsetV = 0.0f;
    material.metallicTextureUvScaleV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-metallic-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scaledVTop = pixelAt(16, 8);
    const auto scaledVBottom = pixelAt(16, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(scaledVTop[channel], scaledVBottom[channel], 0.035f)
            << "metallic-map V scale should compress samples into one half";
    }
}

TEST_F(RayTracingAcceptanceTest, SelectsIndependentRoughnessTextureUvSet) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-roughness-uv-set", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-roughness-uv-set"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data(), nullptr,
        mirroredUvs1.data()));

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    const QString roughnessMapPath = textureDirectory.filePath(
        QStringLiteral("independent-roughness.png"));
    QImage roughnessMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(roughnessMap.isNull());
    for (int y = 0; y < roughnessMap.height(); ++y) {
        auto* row = roughnessMap.scanLine(y);
        for (int x = 0; x < roughnessMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 0 : 255;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(roughnessMap.save(roughnessMapPath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.metallic = 0.0f;
    material.roughness = 1.0f;
    material.roughnessTexture = ArtifactCore::UniString(roughnessMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv0Left = pixelAt(8, 16);
    const auto uv0Right = pixelAt(24, 16);
    const float uv0Difference = std::max(
        std::fabs(uv0Left[0] - uv0Right[0]),
        std::max(std::fabs(uv0Left[1] - uv0Right[1]),
                 std::fabs(uv0Left[2] - uv0Right[2])));
    EXPECT_GT(uv0Difference, 0.02f)
        << "the roughness map's red channel should distinguish the two UV0 halves";

    QImage roughnessOtherChannelsChanged = roughnessMap;
    for (int y = 0; y < roughnessOtherChannelsChanged.height(); ++y) {
        auto* row = roughnessOtherChannelsChanged.scanLine(y);
        for (int x = 0; x < roughnessOtherChannelsChanged.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[1] = 0;
            pixel[2] = 0;
            pixel[3] = 0;
        }
    }
    const QString roughnessOtherChannelsPath = textureDirectory.filePath(
        QStringLiteral("independent-roughness-other-channels.png"));
    ASSERT_TRUE(roughnessOtherChannelsChanged.save(
        roughnessOtherChannelsPath, "PNG"));
    material.roughnessTexture =
        ArtifactCore::UniString(roughnessOtherChannelsPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto otherChannelsChangedLeft = pixelAt(8, 16);
    const auto otherChannelsChangedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(otherChannelsChangedLeft[channel], uv0Left[channel], 0.02f)
            << "roughness should read red independently, left channel "
            << channel;
        EXPECT_NEAR(otherChannelsChangedRight[channel], uv0Right[channel], 0.02f)
            << "roughness should read red independently, right channel "
            << channel;
    }
    material.roughnessTexture = ArtifactCore::UniString(roughnessMapPath);

    const QString halfRoughnessMapPath = textureDirectory.filePath(
        QStringLiteral("independent-roughness-half.png"));
    QImage halfRoughnessMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(halfRoughnessMap.isNull());
    for (int y = 0; y < halfRoughnessMap.height(); ++y) {
        auto* row = halfRoughnessMap.scanLine(y);
        for (int x = 0; x < halfRoughnessMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 0 : 128;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(halfRoughnessMap.save(halfRoughnessMapPath, "PNG"));

    constexpr float halfRoughnessFactor = 128.0f / 255.0f;
    material.roughness = halfRoughnessFactor;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto factorScaledLeft = pixelAt(8, 16);
    const auto factorScaledRight = pixelAt(24, 16);

    material.roughness = 1.0f;
    material.roughnessTexture = ArtifactCore::UniString(halfRoughnessMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto mapScaledLeft = pixelAt(8, 16);
    const auto mapScaledRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(factorScaledLeft[channel], mapScaledLeft[channel], 0.035f)
            << "roughness scalar should multiply the sampled red channel, left "
               "output channel " << channel;
        EXPECT_NEAR(factorScaledRight[channel], mapScaledRight[channel], 0.035f)
            << "roughness scalar and a half-valued map should produce the same "
               "effective factor, output channel " << channel;
    }

    material.roughnessTexture = ArtifactCore::UniString(roughnessMapPath);
    material.roughness = 1.0f;

    material.roughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1Left = pixelAt(8, 16);
    const auto uv1Right = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(uv1Left[channel], uv0Right[channel], 0.035f)
            << "roughness UV1 should mirror the left sample, channel " << channel;
        EXPECT_NEAR(uv1Right[channel], uv0Left[channel], 0.035f)
            << "roughness UV1 should mirror the right sample, channel " << channel;
    }

    material.roughnessTextureUvSet = 0;
    material.roughnessTextureUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto offsetLeft = pixelAt(8, 16);
    const auto offsetRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(offsetLeft[channel], uv0Right[channel], 0.035f)
            << "roughness-map U offset should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(offsetRight[channel], uv0Left[channel], 0.035f)
            << "roughness-map U offset should mirror the right sample, channel "
            << channel;
    }

    material.roughnessTextureUvOffsetU = 0.0f;
    material.roughnessTextureUvScaleU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scaledLeft = pixelAt(8, 16);
    const auto scaledRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(scaledLeft[channel], uv0Left[channel], 0.035f)
            << "roughness-map U scale should keep the left texel on the dark half";
        EXPECT_NEAR(scaledRight[channel], uv0Left[channel], 0.035f)
            << "roughness-map U scale should repeat the dark half on the right";
    }

    material.roughnessTextureUvScaleU = 1.0f;
    material.roughnessTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedLeft = pixelAt(8, 16);
    const auto rotatedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rotatedLeft[channel], uv0Right[channel], 0.035f)
            << "roughness-map 180-degree rotation should mirror the left sample";
        EXPECT_NEAR(rotatedRight[channel], uv0Left[channel], 0.035f)
            << "roughness-map 180-degree rotation should mirror the right sample";
    }

    const QString verticalRoughnessMapPath = textureDirectory.filePath(
        QStringLiteral("independent-roughness-vertical.png"));
    QImage verticalRoughnessMap(2, 8, QImage::Format_RGBA8888);
    ASSERT_FALSE(verticalRoughnessMap.isNull());
    for (int y = 0; y < verticalRoughnessMap.height(); ++y) {
        auto* row = verticalRoughnessMap.scanLine(y);
        const uchar red = y < 4 ? 0 : 255;
        for (int x = 0; x < verticalRoughnessMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = red;
            pixel[1] = pixel[2] = pixel[3] = 255;
        }
    }
    ASSERT_TRUE(verticalRoughnessMap.save(verticalRoughnessMapPath, "PNG"));
    material.roughnessTexture = ArtifactCore::UniString(verticalRoughnessMapPath);
    material.roughnessTextureUvRotationDegrees = 0.0f;
    material.roughnessTextureUvScaleV = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uvVTop = pixelAt(16, 8);
    const auto uvVBottom = pixelAt(16, 24);
    EXPECT_GT(std::fabs(uvVTop[0] - uvVBottom[0]), 0.02f)
        << "the vertical roughness map should distinguish its V samples";

    material.roughnessTextureUvOffsetV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto offsetVTop = pixelAt(16, 8);
    const auto offsetVBottom = pixelAt(16, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(offsetVTop[channel], uvVBottom[channel], 0.035f)
            << "roughness-map V offset should mirror the top sample";
        EXPECT_NEAR(offsetVBottom[channel], uvVTop[channel], 0.035f)
            << "roughness-map V offset should mirror the bottom sample";
    }

    material.roughnessTextureUvOffsetV = 0.0f;
    material.roughnessTextureUvScaleV = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-roughness-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scaledVTop = pixelAt(16, 8);
    const auto scaledVBottom = pixelAt(16, 24);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(scaledVTop[channel], scaledVBottom[channel], 0.035f)
            << "roughness-map V scale should compress samples into one half";
    }
}

TEST_F(RayTracingAcceptanceTest, SamplesSrgbAndHdrEmissionTextures) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-emission-texture", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-emission-texture"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data()));

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString texturePath = temporaryDirectory.filePath(
        QStringLiteral("emission-srgb.png"));
    QImage emissionImage(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(emissionImage.isNull());
    for (int y = 0; y < emissionImage.height(); ++y) {
        auto* row = emissionImage.scanLine(y);
        for (int x = 0; x < emissionImage.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 255 : 0;
            pixel[1] = x < 4 ? 0 : 255;
            pixel[2] = 0;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(emissionImage.save(texturePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.0f;
    material.specular = 0.0f;
    material.ior = 1.0f;
    material.emissionColor[0] = material.emissionColor[1] =
        material.emissionColor[2] = 1.0f;
    material.emissionStrength = 1.0f;
    material.emissionTexture = ArtifactCore::UniString(texturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("26-pbr-emission-texture.png"));
    const auto leftEmission = pixelAt(8, 16);
    const auto rightEmission = pixelAt(24, 16);
    EXPECT_GT(leftEmission[0], leftEmission[1] + 0.2f);
    EXPECT_GT(rightEmission[1], rightEmission[0] + 0.2f);

    QImage emissionWithTransparentTexels = emissionImage;
    for (int y = 0; y < emissionWithTransparentTexels.height(); ++y) {
        auto* row = emissionWithTransparentTexels.scanLine(y);
        for (int x = 0; x < emissionWithTransparentTexels.width() / 2; ++x) {
            row[x * 4 + 3] = 0;
        }
    }
    const QString transparentEmissionPath = temporaryDirectory.filePath(
        QStringLiteral("emission-transparent-texels.png"));
    ASSERT_TRUE(emissionWithTransparentTexels.save(
        transparentEmissionPath, "PNG"));
    material.emissionTexture = ArtifactCore::UniString(transparentEmissionPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto transparentTexelEmission = pixelAt(8, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(transparentTexelEmission[channel], leftEmission[channel],
                    0.01f)
            << "emission texture alpha should not modulate RGB, channel "
            << channel;
    }

    constexpr std::array<float, 8 * 2 * 4> hdrPixels = [] {
        std::array<float, 8 * 2 * 4> pixels{};
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 8; ++x) {
                const size_t offset = static_cast<size_t>(y * 8 + x) * 4;
                pixels[offset + 0] = x < 4 ? 4.0f : 0.0f;
                pixels[offset + 1] = x < 4 ? 0.0f : 2.0f;
                pixels[offset + 2] = x < 4 ? 0.25f : 0.0f;
                pixels[offset + 3] = 1.0f;
            }
        }
        return pixels;
    }();
    const QString hdrTexturePath = temporaryDirectory.filePath(
        QStringLiteral("emission-hdr.exr"));
    ArtifactCore::OpenExr exr;
    ASSERT_TRUE(exr.writeRGBA32F(hdrTexturePath, hdrPixels.data(), 8, 2));
    material.emissionTexture = ArtifactCore::UniString(hdrTexturePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("26-pbr-hdr-emission-texture.exr-source.png"));
    const auto leftHdrEmission = pixelAt(8, 16);
    const auto rightHdrEmission = pixelAt(24, 16);
    EXPECT_GT(leftHdrEmission[0], 3.5f);
    EXPECT_LT(leftHdrEmission[1], 0.1f);
    EXPECT_NEAR(leftHdrEmission[2], 0.25f, 0.02f);
    EXPECT_GT(rightHdrEmission[1], 1.5f);
    EXPECT_LT(rightHdrEmission[0], 0.1f);
    EXPECT_LT(rightHdrEmission[2], 0.02f)
        << "zero-valued HDR texture channels should suppress scalar emission";

    material.emissionColor[0] = 0.5f;
    material.emissionColor[1] = 0.25f;
    material.emissionColor[2] = 0.75f;
    material.emissionStrength = 2.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto modulatedHdrEmission = pixelAt(8, 16);
    EXPECT_NEAR(modulatedHdrEmission[0], 4.0f * 0.214041f * 2.0f, 0.03f)
        << "HDR emission red should multiply the sRGB-decoded scalar color and strength";
    EXPECT_LT(modulatedHdrEmission[1], 0.02f);
    EXPECT_NEAR(modulatedHdrEmission[2], 0.25f * 0.522522f * 2.0f, 0.03f)
        << "HDR emission blue should modulate its independent scalar channel";

    constexpr std::array<float, 8 * 2 * 4> sanitizedHdrPixels = [] {
        std::array<float, 8 * 2 * 4> pixels{};
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 8; ++x) {
                const size_t offset = static_cast<size_t>(y * 8 + x) * 4;
                pixels[offset + 0] = x < 4
                    ? std::numeric_limits<float>::quiet_NaN() : -1.0f;
                pixels[offset + 1] = 0.5f;
                pixels[offset + 2] = 0.25f;
                pixels[offset + 3] = 1.0f;
            }
        }
        return pixels;
    }();
    const QString sanitizedHdrTexturePath = temporaryDirectory.filePath(
        QStringLiteral("emission-sanitized.exr"));
    ASSERT_TRUE(exr.writeRGBA32F(sanitizedHdrTexturePath,
                                 sanitizedHdrPixels.data(), 8, 2));
    material.emissionTexture = ArtifactCore::UniString(sanitizedHdrTexturePath);
    material.emissionColor[0] = material.emissionColor[1] =
        material.emissionColor[2] = 1.0f;
    material.emissionStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto nonFiniteHdrEmission = pixelAt(8, 16);
    const auto negativeHdrEmission = pixelAt(24, 16);
    EXPECT_NEAR(nonFiniteHdrEmission[0], 0.0f, 1.0e-5f)
        << "non-finite HDR emission texture red should sanitize to zero";
    EXPECT_NEAR(negativeHdrEmission[0], 0.0f, 1.0e-5f)
        << "negative HDR emission texture red should sanitize to zero";
    EXPECT_NEAR(nonFiniteHdrEmission[1], 0.5f, 0.02f);
    EXPECT_NEAR(nonFiniteHdrEmission[2], 0.25f, 0.02f);
    EXPECT_NEAR(negativeHdrEmission[1], 0.5f, 0.02f);
    EXPECT_NEAR(negativeHdrEmission[2], 0.25f, 0.02f);

    material.emissionColor[0] = material.emissionColor[1] =
        material.emissionColor[2] = 1.0f;
    material.emissionStrength = 1.0f;
    material.emissionTexture = ArtifactCore::UniString();
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scalarEmission = pixelAt(16, 16);

    material.emissionTexture = ArtifactCore::UniString(
        temporaryDirectory.filePath(QStringLiteral("missing-emission.png")));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto missingTextureEmission = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(missingTextureEmission[channel], scalarEmission[channel],
                    0.01f)
            << "a missing emission texture should preserve scalar emission, "
               "channel " << channel;
    }

    material.emissionTexture = ArtifactCore::UniString();
    material.emissionColor[0] = 0.5f;
    material.emissionColor[1] = 0.25f;
    material.emissionColor[2] = 0.0f;
    material.emissionStrength = 2.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scalarColorEmission = pixelAt(16, 16);
    EXPECT_NEAR(scalarColorEmission[0], 0.42808f, 0.01f)
        << "scalar emission red should convert sRGB 0.5 to linear and scale by strength";
    EXPECT_NEAR(scalarColorEmission[1], 0.10175f, 0.01f)
        << "scalar emission green should convert sRGB 0.25 to linear and scale by strength";
    EXPECT_NEAR(scalarColorEmission[2], 0.0f, 0.01f);

    material.emissionStrength = -1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto negativeEmissionStrength = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(negativeEmissionStrength[channel], 0.0f, 1.0e-5f)
            << "negative emission strength should clamp to zero, channel "
            << channel;
    }

    material.emissionStrength = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto zeroEmissionStrength = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(zeroEmissionStrength[channel], 0.0f, 1.0e-5f)
            << "zero emission strength should suppress scalar emission, channel "
            << channel;
    }

    material.clearcoat = 1.0f;
    material.clearcoatRoughness = 0.2f;
    material.sheen = 1.0f;
    material.sheenRoughness = 0.25f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto layeredWithoutEmission = pixelAt(16, 16);

    material.emissionStrength = 2.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-emission-texture"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto layeredWithEmission = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const float layeredEmissionContribution =
            layeredWithEmission[channel] - layeredWithoutEmission[channel];
        EXPECT_TRUE(std::isfinite(layeredEmissionContribution))
            << "layered emission contribution should remain finite, channel "
            << channel;
        EXPECT_GE(layeredEmissionContribution, 0.0f)
            << "layered emission contribution should be non-negative, channel "
            << channel;
        EXPECT_LE(layeredEmissionContribution, scalarColorEmission[channel] +
                                                  0.01f)
            << "coat and sheen should not amplify emission beyond its bare "
            << "response, channel " << channel;
    }
    EXPECT_LT(layeredWithEmission[0] - layeredWithoutEmission[0],
              scalarColorEmission[0] - 0.001f)
        << "clearcoat and sheen should attenuate emitted red energy";
    EXPECT_LT(layeredWithEmission[1] - layeredWithoutEmission[1],
              scalarColorEmission[1] - 0.001f)
        << "clearcoat and sheen should attenuate emitted green energy";
}

TEST_F(RayTracingAcceptanceTest, SelectsSheenTextureUvSets) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-sheen-texture-uv-set", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data(), nullptr,
        mirroredUvs1.data()));

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    const QString sheenColorMapPath = textureDirectory.filePath(
        QStringLiteral("sheen-color.png"));
    const QString sheenRoughnessMapPath = textureDirectory.filePath(
        QStringLiteral("sheen-roughness-alpha.png"));
    QImage sheenColorMap(8, 2, QImage::Format_RGBA8888);
    QImage sheenRoughnessMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(sheenColorMap.isNull());
    ASSERT_FALSE(sheenRoughnessMap.isNull());
    for (int y = 0; y < sheenColorMap.height(); ++y) {
        auto* row = sheenColorMap.scanLine(y);
        auto* roughnessRow = sheenRoughnessMap.scanLine(y);
        for (int x = 0; x < sheenColorMap.width(); ++x) {
            auto* pixel = row + x * 4;
            const uchar value = x < 4 ? 0 : 255;
            pixel[0] = pixel[1] = pixel[2] = value;
            pixel[3] = 255;

            auto* roughnessPixel = roughnessRow + x * 4;
            roughnessPixel[0] = 255;
            roughnessPixel[1] = 0;
            roughnessPixel[2] = 255;
            roughnessPixel[3] = value;
        }
    }
    ASSERT_TRUE(sheenColorMap.save(sheenColorMapPath, "PNG"));
    ASSERT_TRUE(sheenRoughnessMap.save(sheenRoughnessMapPath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.sheen = 1.0f;
    material.sheenRoughness = 0.5f;
    material.sheenColorTexture = ArtifactCore::UniString(sheenColorMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv0Left = pixelAt(8, 16);
    const auto uv0Right = pixelAt(24, 16);
    const float uv0Difference = std::max(
        std::fabs(uv0Left[0] - uv0Right[0]),
        std::max(std::fabs(uv0Left[1] - uv0Right[1]),
                 std::fabs(uv0Left[2] - uv0Right[2])));
    EXPECT_GT(uv0Difference, 0.02f)
        << "the sheen color map should distinguish the two UV0 halves";

    material.sheenColorTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedColorLeft = pixelAt(8, 16);
    const auto rotatedColorRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rotatedColorLeft[channel], uv0Right[channel], 0.035f)
            << "sheen color UV rotation should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(rotatedColorRight[channel], uv0Left[channel], 0.035f)
            << "sheen color UV rotation should mirror the right sample, channel "
            << channel;
    }
    material.sheenColorTextureUvRotationDegrees = 0.0f;

    material.sheenColorTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1Left = pixelAt(8, 16);
    const auto uv1Right = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(uv1Left[channel], uv0Right[channel], 0.035f)
            << "sheen color UV1 should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(uv1Right[channel], uv0Left[channel], 0.035f)
            << "sheen color UV1 should mirror the right sample, channel "
            << channel;
    }

    material.sheenColorTexture = ArtifactCore::UniString();
    material.sheenRoughness = 1.0f;
    material.sheenRoughnessTexture =
        ArtifactCore::UniString(sheenRoughnessMapPath);
    material.sheenRoughnessTextureUvSet = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughnessUv0Left = pixelAt(8, 16);
    const auto roughnessUv0Right = pixelAt(24, 16);
    const float roughnessUv0Difference = std::max(
        std::fabs(roughnessUv0Left[0] - roughnessUv0Right[0]),
        std::max(std::fabs(roughnessUv0Left[1] - roughnessUv0Right[1]),
                 std::fabs(roughnessUv0Left[2] - roughnessUv0Right[2])));
    EXPECT_GT(roughnessUv0Difference, 0.02f)
        << "the sheen roughness map should distinguish the two UV0 halves";

    QImage sheenRoughnessOtherChannelsChanged = sheenRoughnessMap;
    for (int y = 0; y < sheenRoughnessOtherChannelsChanged.height(); ++y) {
        auto* row = sheenRoughnessOtherChannelsChanged.scanLine(y);
        for (int x = 0; x < sheenRoughnessOtherChannelsChanged.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 0;
            pixel[1] = 255;
            pixel[2] = 0;
        }
    }
    const QString sheenRoughnessOtherChannelsPath = textureDirectory.filePath(
        QStringLiteral("sheen-roughness-other-channels.png"));
    ASSERT_TRUE(sheenRoughnessOtherChannelsChanged.save(
        sheenRoughnessOtherChannelsPath, "PNG"));
    material.sheenRoughnessTexture =
        ArtifactCore::UniString(sheenRoughnessOtherChannelsPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto otherChannelsChangedLeft = pixelAt(8, 16);
    const auto otherChannelsChangedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(otherChannelsChangedLeft[channel], roughnessUv0Left[channel],
                    0.02f)
            << "sheen roughness should read alpha independently, left channel "
            << channel;
        EXPECT_NEAR(otherChannelsChangedRight[channel],
                    roughnessUv0Right[channel], 0.02f)
            << "sheen roughness should read alpha independently, right channel "
            << channel;
    }

    material.sheenRoughnessTextureUvSet = 1;
    material.sheenRoughnessTexture =
        ArtifactCore::UniString(sheenRoughnessMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughnessUv1Left = pixelAt(8, 16);
    const auto roughnessUv1Right = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(roughnessUv1Left[channel], roughnessUv0Right[channel],
                    0.035f)
            << "sheen roughness UV1 should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(roughnessUv1Right[channel], roughnessUv0Left[channel],
                    0.035f)
            << "sheen roughness UV1 should mirror the right sample, channel "
            << channel;
    }

    material.sheenRoughnessTextureUvSet = 0;
    material.sheenRoughness = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfScalarFullAlpha = pixelAt(24, 16);

    QImage halfAlphaSheenRoughnessMap = sheenRoughnessMap;
    for (int y = 0; y < halfAlphaSheenRoughnessMap.height(); ++y) {
        auto* row = halfAlphaSheenRoughnessMap.scanLine(y);
        for (int x = halfAlphaSheenRoughnessMap.width() / 2;
             x < halfAlphaSheenRoughnessMap.width(); ++x) {
            row[x * 4 + 3] = 128;
        }
    }
    const QString halfAlphaSheenRoughnessMapPath = textureDirectory.filePath(
        QStringLiteral("sheen-roughness-half-alpha.png"));
    ASSERT_TRUE(halfAlphaSheenRoughnessMap.save(
        halfAlphaSheenRoughnessMapPath, "PNG"));
    material.sheenRoughness = 1.0f;
    material.sheenRoughnessTexture =
        ArtifactCore::UniString(halfAlphaSheenRoughnessMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfAlphaFullScalar = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(halfAlphaFullScalar[channel], halfScalarFullAlpha[channel],
                    0.035f)
            << "sheen roughness scalar should multiply texture alpha, channel "
            << channel;
    }

    const QString whiteSheenColorPath = textureDirectory.filePath(
        QStringLiteral("sheen-color-white.png"));
    const QString halfLinearSheenColorPath = textureDirectory.filePath(
        QStringLiteral("sheen-color-half-linear.png"));
    QImage whiteSheenColor(2, 2, QImage::Format_RGBA8888);
    QImage halfLinearSheenColor(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(whiteSheenColor.isNull());
    ASSERT_FALSE(halfLinearSheenColor.isNull());
    whiteSheenColor.fill(QColor(255, 255, 255, 255));
    halfLinearSheenColor.fill(QColor(188, 188, 188, 255));
    ASSERT_TRUE(whiteSheenColor.save(whiteSheenColorPath, "PNG"));
    ASSERT_TRUE(halfLinearSheenColor.save(halfLinearSheenColorPath, "PNG"));
    material.sheenRoughnessTexture = ArtifactCore::UniString();
    material.sheenRoughness = 0.5f;
    material.sheenColorTexture = ArtifactCore::UniString(whiteSheenColorPath);
    material.sheenTint[0] = material.sheenTint[1] =
        material.sheenTint[2] = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfTintWhiteTexture = pixelAt(16, 16);

    material.sheenColorTexture =
        ArtifactCore::UniString(halfLinearSheenColorPath);
    material.sheenTint[0] = material.sheenTint[1] =
        material.sheenTint[2] = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfLinearTextureWhiteTint = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(halfLinearTextureWhiteTint[channel],
                    halfTintWhiteTexture[channel], 0.025f)
            << "sheen tint should multiply the linearized color texture, channel "
            << channel;
    }

    QImage transparentSheenColor = halfLinearSheenColor;
    for (int y = 0; y < transparentSheenColor.height(); ++y) {
        auto* row = transparentSheenColor.scanLine(y);
        for (int x = 0; x < transparentSheenColor.width(); ++x) {
            row[x * 4 + 3] = 0;
        }
    }
    const QString transparentSheenColorPath = textureDirectory.filePath(
        QStringLiteral("sheen-color-transparent.png"));
    ASSERT_TRUE(transparentSheenColor.save(transparentSheenColorPath, "PNG"));
    material.sheenColorTexture = ArtifactCore::UniString(transparentSheenColorPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto transparentMapPixel = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(transparentMapPixel[channel],
                    halfLinearTextureWhiteTint[channel], 0.02f)
            << "sheen color alpha should not affect its RGB response, channel "
            << channel;
    }

    material.sheen = 1.0f;
    material.sheenTint[0] = material.sheenTint[1] =
        material.sheenTint[2] = 1.0f;
    material.sheenColorTexture = ArtifactCore::UniString(whiteSheenColorPath);
    material.sheenColorTextureUvSet = 0;
    material.sheenRoughness = 0.0f;
    material.sheenRoughnessTexture = ArtifactCore::UniString();
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto smoothSheenReference = pixelAt(24, 16);

    material.sheen = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto noSheenReferenceLeft = pixelAt(8, 16);

    material.sheen = 1.0f;
    material.sheenRoughness = 1.0f;
    material.sheenColorTexture = ArtifactCore::UniString(sheenColorMapPath);
    material.sheenColorTextureUvSet = 0;
    material.sheenRoughnessTexture =
        ArtifactCore::UniString(sheenRoughnessMapPath);
    material.sheenRoughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto splitUvLeft = pixelAt(8, 16);
    const auto splitUvRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(splitUvLeft[channel], noSheenReferenceLeft[channel], 0.035f)
            << "black UV0 sheen color should suppress sheen even when roughness "
               "uses UV1, channel "
            << channel;
        EXPECT_NEAR(splitUvRight[channel], smoothSheenReference[channel],
                    0.035f)
            << "white UV0 sheen color and mirrored UV1 alpha should select the "
               "smooth sheen sample, channel "
            << channel;
    }

    material.sheenRoughnessTextureUvSet = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto sameUvSetRight = pixelAt(24, 16);
    const float roughnessUvSelectorResponse = std::max(
        std::fabs(sameUvSetRight[0] - splitUvRight[0]),
        std::max(std::fabs(sameUvSetRight[1] - splitUvRight[1]),
                 std::fabs(sameUvSetRight[2] - splitUvRight[2])));
    EXPECT_GT(roughnessUvSelectorResponse, 0.001f)
        << "changing only the sheen roughness UV set should alter its "
           "combined-map shading";

    material.sheenRoughnessTextureUvSet = 1;
    material.sheenColorTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-sheen-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto mirroredColorLeft = pixelAt(8, 16);
    const auto mirroredColorRight = pixelAt(24, 16);
    EXPECT_GT(std::fabs(mirroredColorLeft[0] - splitUvLeft[0]), 0.001f)
        << "changing only the sheen color UV set should alter the left sample";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(mirroredColorRight[channel], noSheenReferenceLeft[channel],
                    0.035f)
            << "black mirrored sheen color should suppress sheen independently "
               "of its UV1 roughness map, channel "
            << channel;
    }
}

TEST_F(RayTracingAcceptanceTest, SelectsClearcoatTextureUvSets) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-clearcoat-texture-uv-set", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data(),
        nullptr, mirroredUvs1.data()));

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    const QString clearcoatMapPath = textureDirectory.filePath(
        QStringLiteral("clearcoat-ramp.png"));
    const QString clearcoatRoughnessMapPath = textureDirectory.filePath(
        QStringLiteral("clearcoat-roughness-ramp.png"));
    QImage clearcoatMap(8, 2, QImage::Format_RGBA8888);
    QImage clearcoatRoughnessMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(clearcoatMap.isNull());
    ASSERT_FALSE(clearcoatRoughnessMap.isNull());
    for (int y = 0; y < clearcoatMap.height(); ++y) {
        auto* row = clearcoatMap.scanLine(y);
        auto* roughnessRow = clearcoatRoughnessMap.scanLine(y);
        for (int x = 0; x < clearcoatMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = x < 4 ? 0 : 255;
            pixel[1] = 0;
            pixel[2] = 255;
            pixel[3] = 255;

            auto* roughnessPixel = roughnessRow + x * 4;
            roughnessPixel[0] = 0;
            roughnessPixel[1] = x < 4 ? 0 : 255;
            roughnessPixel[2] = 255;
            roughnessPixel[3] = 255;
        }
    }
    ASSERT_TRUE(clearcoatMap.save(clearcoatMapPath, "PNG"));
    ASSERT_TRUE(clearcoatRoughnessMap.save(clearcoatRoughnessMapPath, "PNG"));
    const QString smoothRoughnessPath = textureDirectory.filePath(
        QStringLiteral("clearcoat-roughness-smooth.png"));
    const QString roughRoughnessPath = textureDirectory.filePath(
        QStringLiteral("clearcoat-roughness-rough.png"));
    QImage smoothRoughnessMap(8, 2, QImage::Format_RGBA8888);
    QImage roughRoughnessMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(smoothRoughnessMap.isNull());
    ASSERT_FALSE(roughRoughnessMap.isNull());
    smoothRoughnessMap.fill(QColor(128, 0, 255, 255));
    roughRoughnessMap.fill(QColor(128, 255, 255, 255));
    ASSERT_TRUE(smoothRoughnessMap.save(smoothRoughnessPath, "PNG"));
    ASSERT_TRUE(roughRoughnessMap.save(roughRoughnessPath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.roughness = 0.4f;
    material.clearcoat = 1.0f;
    material.clearcoatRoughness = 0.2f;
    material.clearcoatTexture = ArtifactCore::UniString(clearcoatMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto amountUv0Left = pixelAt(8, 16);
    const auto amountUv0Right = pixelAt(24, 16);
    const float amountUv0Difference = std::max(
        std::fabs(amountUv0Left[0] - amountUv0Right[0]),
        std::max(std::fabs(amountUv0Left[1] - amountUv0Right[1]),
                 std::fabs(amountUv0Left[2] - amountUv0Right[2])));
    EXPECT_GT(amountUv0Difference, 0.02f)
        << "the clearcoat map should distinguish the two UV0 halves";

    QImage clearcoatOtherChannelsChanged = clearcoatMap;
    for (int y = 0; y < clearcoatOtherChannelsChanged.height(); ++y) {
        auto* row = clearcoatOtherChannelsChanged.scanLine(y);
        for (int x = 0; x < clearcoatOtherChannelsChanged.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[1] = 255;
            pixel[2] = 0;
            pixel[3] = 0;
        }
    }
    const QString clearcoatOtherChannelsChangedPath = textureDirectory.filePath(
        QStringLiteral("clearcoat-other-channels-changed.png"));
    ASSERT_TRUE(clearcoatOtherChannelsChanged.save(
        clearcoatOtherChannelsChangedPath, "PNG"));
    material.clearcoatTexture =
        ArtifactCore::UniString(clearcoatOtherChannelsChangedPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto changedOtherChannelsLeft = pixelAt(8, 16);
    const auto changedOtherChannelsRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(changedOtherChannelsLeft[channel], amountUv0Left[channel],
                    0.02f)
            << "clearcoat factor should read red independently, left channel "
            << channel;
        EXPECT_NEAR(changedOtherChannelsRight[channel], amountUv0Right[channel],
                    0.02f)
            << "clearcoat factor should read red independently, right channel "
            << channel;
    }

    material.clearcoatTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto amountUv1Left = pixelAt(8, 16);
    const auto amountUv1Right = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(amountUv1Left[channel], amountUv0Right[channel], 0.035f)
            << "clearcoat UV1 should mirror the left sample, channel " << channel;
        EXPECT_NEAR(amountUv1Right[channel], amountUv0Left[channel], 0.035f)
            << "clearcoat UV1 should mirror the right sample, channel " << channel;
    }

    material.clearcoatTexture = ArtifactCore::UniString(clearcoatMapPath);
    material.clearcoatTextureUvSet = 0;
    material.clearcoatTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto amountRotatedLeft = pixelAt(8, 16);
    const auto amountRotatedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(amountRotatedLeft[channel], amountUv0Right[channel], 0.035f)
            << "clearcoat amount UV rotation should mirror the left sample, "
               "channel "
            << channel;
        EXPECT_NEAR(amountRotatedRight[channel], amountUv0Left[channel], 0.035f)
            << "clearcoat amount UV rotation should mirror the right sample, "
               "channel "
            << channel;
    }
    material.clearcoatTextureUvRotationDegrees = 0.0f;

    material.clearcoatTextureUvSet = 0;
    material.clearcoat = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto disabledMapLeft = pixelAt(8, 16);
    const auto disabledMapRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(disabledMapLeft[channel], disabledMapRight[channel], 0.035f)
            << "zero clearcoat factor should disable the amount map, channel "
            << channel;
    }

    material.clearcoat = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfFactorLeft = pixelAt(8, 16);
    const auto halfFactorRight = pixelAt(24, 16);
    const float halfFactorDifference = std::max(
        std::fabs(halfFactorLeft[0] - halfFactorRight[0]),
        std::max(std::fabs(halfFactorLeft[1] - halfFactorRight[1]),
                 std::fabs(halfFactorLeft[2] - halfFactorRight[2])));
    EXPECT_GT(halfFactorDifference, 0.005f)
        << "a partial clearcoat factor should retain a map response";
    EXPECT_LT(halfFactorDifference, amountUv0Difference)
        << "a partial clearcoat factor should reduce the full-factor map response";

    QImage halfRedClearcoatMap = clearcoatMap;
    for (int y = 0; y < halfRedClearcoatMap.height(); ++y) {
        auto* row = halfRedClearcoatMap.scanLine(y);
        for (int x = halfRedClearcoatMap.width() / 2;
             x < halfRedClearcoatMap.width(); ++x) {
            row[x * 4] = 128;
        }
    }
    const QString halfRedClearcoatMapPath = textureDirectory.filePath(
        QStringLiteral("clearcoat-half-red.png"));
    ASSERT_TRUE(halfRedClearcoatMap.save(halfRedClearcoatMapPath, "PNG"));
    material.clearcoat = 1.0f;
    material.clearcoatTexture = ArtifactCore::UniString(halfRedClearcoatMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfMapRedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(halfMapRedRight[channel], halfFactorRight[channel], 0.035f)
            << "clearcoat factor should multiply texture red, channel "
            << channel;
    }

    material.clearcoatTexture = ArtifactCore::UniString();
    material.clearcoatRoughness = 1.0f;
    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(clearcoatRoughnessMapPath);
    material.clearcoatRoughnessTextureUvSet = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughnessUv0Left = pixelAt(8, 16);
    const auto roughnessUv0Right = pixelAt(24, 16);
    const float roughnessUv0Difference = std::max(
        std::fabs(roughnessUv0Left[0] - roughnessUv0Right[0]),
        std::max(std::fabs(roughnessUv0Left[1] - roughnessUv0Right[1]),
                 std::fabs(roughnessUv0Left[2] - roughnessUv0Right[2])));
    EXPECT_GT(roughnessUv0Difference, 0.02f)
        << "the clearcoat roughness map should distinguish the UV0 halves";

    QImage clearcoatRoughnessOtherChannelsChanged = clearcoatRoughnessMap;
    for (int y = 0; y < clearcoatRoughnessOtherChannelsChanged.height(); ++y) {
        auto* row = clearcoatRoughnessOtherChannelsChanged.scanLine(y);
        for (int x = 0; x < clearcoatRoughnessOtherChannelsChanged.width();
             ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 255;
            pixel[2] = 0;
            pixel[3] = 0;
        }
    }
    const QString clearcoatRoughnessOtherChannelsPath =
        textureDirectory.filePath(
            QStringLiteral("clearcoat-roughness-other-channels.png"));
    ASSERT_TRUE(clearcoatRoughnessOtherChannelsChanged.save(
        clearcoatRoughnessOtherChannelsPath, "PNG"));
    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(clearcoatRoughnessOtherChannelsPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto changedRoughnessOtherChannelsLeft = pixelAt(8, 16);
    const auto changedRoughnessOtherChannelsRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(changedRoughnessOtherChannelsLeft[channel],
                    roughnessUv0Left[channel], 0.02f)
            << "clearcoat roughness should read green independently, left "
            << "channel " << channel;
        EXPECT_NEAR(changedRoughnessOtherChannelsRight[channel],
                    roughnessUv0Right[channel], 0.02f)
            << "clearcoat roughness should read green independently, right "
            << "channel " << channel;
    }

    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(smoothRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto smoothRoughnessLeft = pixelAt(8, 16);
    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(roughRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughRoughnessRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(roughnessUv0Left[channel], smoothRoughnessLeft[channel],
                    0.035f)
            << "clearcoat roughness should sample the G=0 reference, channel "
            << channel;
        EXPECT_NEAR(roughnessUv0Right[channel], roughRoughnessRight[channel],
                    0.035f)
            << "clearcoat roughness should sample the G=1 reference, channel "
            << channel;
    }

    material.clearcoatRoughness = 0.5f;
    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(roughRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfScalarFullGreen = pixelAt(24, 16);

    QImage halfGreenClearcoatRoughnessMap = roughRoughnessMap;
    for (int y = 0; y < halfGreenClearcoatRoughnessMap.height(); ++y) {
        auto* row = halfGreenClearcoatRoughnessMap.scanLine(y);
        for (int x = halfGreenClearcoatRoughnessMap.width() / 2;
             x < halfGreenClearcoatRoughnessMap.width(); ++x) {
            row[x * 4 + 1] = 128;
        }
    }
    const QString halfGreenClearcoatRoughnessPath = textureDirectory.filePath(
        QStringLiteral("clearcoat-roughness-half-green.png"));
    ASSERT_TRUE(halfGreenClearcoatRoughnessMap.save(
        halfGreenClearcoatRoughnessPath, "PNG"));
    material.clearcoatRoughness = 1.0f;
    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(halfGreenClearcoatRoughnessPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfGreenFullScalar = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(halfGreenFullScalar[channel], halfScalarFullGreen[channel],
                    0.035f)
            << "clearcoat roughness scalar should multiply texture green, "
            << "channel " << channel;
    }

    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(clearcoatRoughnessMapPath);
    material.clearcoatRoughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughnessUv1Left = pixelAt(8, 16);
    const auto roughnessUv1Right = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(roughnessUv1Left[channel], roughnessUv0Right[channel],
                    0.035f)
            << "clearcoat roughness UV1 should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(roughnessUv1Right[channel], roughnessUv0Left[channel],
                    0.035f)
            << "clearcoat roughness UV1 should mirror the right sample, channel "
            << channel;
    }

    material.clearcoatRoughnessTextureUvSet = 0;
    material.clearcoatRoughnessTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughnessRotatedLeft = pixelAt(8, 16);
    const auto roughnessRotatedRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(roughnessRotatedLeft[channel], roughnessUv0Right[channel],
                    0.035f)
            << "clearcoat roughness UV rotation should mirror the left sample, "
               "channel "
            << channel;
        EXPECT_NEAR(roughnessRotatedRight[channel], roughnessUv0Left[channel],
                    0.035f)
            << "clearcoat roughness UV rotation should mirror the right sample, "
               "channel "
            << channel;
    }
    material.clearcoatRoughnessTextureUvRotationDegrees = 0.0f;

    material.clearcoatTexture = ArtifactCore::UniString();
    material.clearcoat = 1.0f;
    material.clearcoatRoughnessTexture = ArtifactCore::UniString();
    material.clearcoatRoughness = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto smoothClearcoatReference = pixelAt(24, 16);

    material.clearcoatRoughness = 1.0f;
    material.clearcoatTexture = ArtifactCore::UniString(clearcoatMapPath);
    material.clearcoatTextureUvSet = 0;
    material.clearcoatRoughnessTexture =
        ArtifactCore::UniString(clearcoatRoughnessMapPath);
    material.clearcoatRoughnessTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto splitUvLeft = pixelAt(8, 16);
    const auto splitUvRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(splitUvLeft[channel], disabledMapLeft[channel], 0.035f)
            << "zero clearcoat factor-map red should disable the coat even when "
               "roughness uses UV1, channel "
            << channel;
        EXPECT_NEAR(splitUvRight[channel], smoothClearcoatReference[channel],
                    0.035f)
            << "UV0 clearcoat factor and mirrored UV1 roughness should select "
               "the smooth coat sample, channel "
            << channel;
    }

    material.clearcoatRoughnessTextureUvSet = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-clearcoat-texture-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto sameUvSetRight = pixelAt(24, 16);
    const float roughnessUvSelectorResponse = std::max(
        std::fabs(sameUvSetRight[0] - splitUvRight[0]),
        std::max(std::fabs(sameUvSetRight[1] - splitUvRight[1]),
                 std::fabs(sameUvSetRight[2] - splitUvRight[2])));
    EXPECT_GT(roughnessUvSelectorResponse, 0.001f)
        << "changing only the clearcoat roughness UV set should alter its "
           "combined-map shading";
}

TEST_F(RayTracingAcceptanceTest,
       SelectsMipLevelsForMinifiedSheenColorAndRoughnessTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-sheen-mips", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-sheen-mips"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("sheen-checker.png"));
    const auto colorAveragePath =
        tempDirectory.filePath(QStringLiteral("sheen-color-average.png"));
    const auto roughnessAveragePath =
        tempDirectory.filePath(QStringLiteral("sheen-roughness-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage colorAverage(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage roughnessAverage(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(colorAverage.isNull());
    ASSERT_FALSE(roughnessAverage.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* colorAverageRow = colorAverage.scanLine(y);
        auto* roughnessAverageRow = roughnessAverage.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            const uchar value = ((x + y) & 1) != 0 ? 255 : 0;
            auto* checkerPixel = checkerRow + x * 4;
            auto* colorPixel = colorAverageRow + x * 4;
            auto* roughnessPixel = roughnessAverageRow + x * 4;
            checkerPixel[0] = checkerPixel[1] = checkerPixel[2] = value;
            colorPixel[0] = colorPixel[1] = colorPixel[2] = 188;
            roughnessPixel[0] = roughnessPixel[1] = roughnessPixel[2] = 127;
            checkerPixel[3] = colorPixel[3] = roughnessPixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(colorAverage.save(colorAveragePath, "PNG"));
    ASSERT_TRUE(roughnessAverage.save(roughnessAveragePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.sheen = 1.0f;
    material.sheenRoughness = 0.5f;
    material.sheenColorTexture = ArtifactCore::UniString(checkerPath);
    const auto traceAndSample = [&](const ArtifactCore::RTPbrMaterial& input,
                                    const int x) {
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-sheen-mips"), input));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(x, 16)[0];
    };
    std::array<float, 5> checkerSamples{};
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }

    material.sheenColorTexture = ArtifactCore::UniString(colorAveragePath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)],
                    traceAndSample(material, 14 + index), 0.035f)
            << "minified sheen-color sample " << index
            << " should match its linear-light sRGB mip average";
    }

    material.sheenColorTexture = ArtifactCore::UniString();
    material.sheenRoughnessTexture = ArtifactCore::UniString(checkerPath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }
    material.sheenRoughnessTexture = ArtifactCore::UniString(roughnessAveragePath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)],
                    traceAndSample(material, 14 + index), 0.035f)
            << "minified sheen-roughness sample " << index
            << " should match its linear scalar mip average";
    }
}

TEST_F(RayTracingAcceptanceTest,
       SelectsMipLevelsForMinifiedAnisotropyTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-anisotropy-mip", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-anisotropy-mip"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("anisotropy-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("anisotropy-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage average(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = averagePixel[0] = 128;
            checkerPixel[1] = averagePixel[1] = 128;
            checkerPixel[2] = ((x + y) & 1) != 0 ? 255 : 0;
            averagePixel[2] = 127;
            checkerPixel[3] = averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = 0.8f;
    material.baseColor[1] = 0.5f;
    material.baseColor[2] = 0.2f;
    material.metallic = 1.0f;
    material.roughness = 0.24f;
    material.anisotropy = 1.0f;
    material.anisotropyTexture = ArtifactCore::UniString(checkerPath);
    const auto traceAndSample = [&](const ArtifactCore::RTPbrMaterial& input,
                                    const int x) {
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-anisotropy-mip"), input));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(x, 16)[0];
    };
    std::array<float, 5> checkerSamples{};
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }

    material.anisotropyTexture = ArtifactCore::UniString(averagePath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)],
                    traceAndSample(material, 14 + index), 0.04f)
            << "minified anisotropy sample " << index
            << " should match its linear mip average";
    }
}

TEST_F(RayTracingAcceptanceTest, SelectsAnisotropyTextureUvSet) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-anisotropy-uv-set", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data(), nullptr,
        mirroredUvs1.data()));

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    const QString anisotropyMapPath = textureDirectory.filePath(
        QStringLiteral("anisotropy.png"));
    QImage anisotropyMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(anisotropyMap.isNull());
    for (int y = 0; y < anisotropyMap.height(); ++y) {
        auto* row = anisotropyMap.scanLine(y);
        for (int x = 0; x < anisotropyMap.width(); ++x) {
            auto* pixel = row + x * 4;
            pixel[0] = 255;
            pixel[1] = 128;
            pixel[2] = x < 4 ? 0 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(anisotropyMap.save(anisotropyMapPath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = 0.8f;
    material.baseColor[1] = 0.5f;
    material.baseColor[2] = 0.2f;
    material.metallic = 1.0f;
    material.roughness = 0.24f;
    material.anisotropy = 1.0f;
    material.anisotropyTexture = ArtifactCore::UniString(anisotropyMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv0Left = pixelAt(8, 16);
    const auto uv0Right = pixelAt(24, 16);
    const float uv0Difference = std::max(
        std::fabs(uv0Left[0] - uv0Right[0]),
        std::max(std::fabs(uv0Left[1] - uv0Right[1]),
                 std::fabs(uv0Left[2] - uv0Right[2])));
    EXPECT_GT(uv0Difference, 0.02f)
        << "the anisotropy map's blue channel should distinguish the UV0 halves";

    material.anisotropyTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1Left = pixelAt(8, 16);
    const auto uv1Right = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(uv1Left[channel], uv0Right[channel], 0.04f)
            << "anisotropy UV1 should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(uv1Right[channel], uv0Left[channel], 0.04f)
            << "anisotropy UV1 should mirror the right sample, channel "
            << channel;
    }

    const QString anisotropyDirectionMapPath = textureDirectory.filePath(
        QStringLiteral("anisotropy-direction.png"));
    QImage anisotropyDirectionMap(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(anisotropyDirectionMap.isNull());
    for (int y = 0; y < anisotropyDirectionMap.height(); ++y) {
        auto* row = anisotropyDirectionMap.scanLine(y);
        for (int x = 0; x < anisotropyDirectionMap.width(); ++x) {
            auto* pixel = row + x * 4;
            const bool vertical = x >= anisotropyDirectionMap.width() / 2;
            pixel[0] = vertical ? 128 : 255;
            pixel[1] = vertical ? 255 : 128;
            pixel[2] = 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(anisotropyDirectionMap.save(anisotropyDirectionMapPath, "PNG"));
    material.anisotropyTexture = ArtifactCore::UniString(
        anisotropyDirectionMapPath);
    material.anisotropyTextureUvSet = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto horizontalDirectionPixel = pixelAt(8, 16);
    const auto verticalDirectionPixel = pixelAt(24, 16);
    const float anisotropyDirectionDifference = std::max(
        std::fabs(horizontalDirectionPixel[0] - verticalDirectionPixel[0]),
        std::max(std::fabs(horizontalDirectionPixel[1] - verticalDirectionPixel[1]),
                 std::fabs(horizontalDirectionPixel[2] - verticalDirectionPixel[2])));
    EXPECT_GT(anisotropyDirectionDifference, 0.001f)
        << "anisotropy texture red/green direction channels should rotate the "
           "specular lobe while blue strength stays constant";

    material.anisotropyTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedMapLeft = pixelAt(8, 16);
    const auto rotatedMapRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rotatedMapLeft[channel], verticalDirectionPixel[channel],
                    0.04f)
            << "anisotropy texture UV rotation should swap sampled texels while "
            << "preserving the shading tangent frame, left channel " << channel;
        EXPECT_NEAR(rotatedMapRight[channel], horizontalDirectionPixel[channel],
                    0.04f)
            << "anisotropy texture UV rotation should swap sampled texels while "
            << "preserving the shading tangent frame, right channel " << channel;
    }

    material.anisotropyTextureUvRotationDegrees = 0.0f;
    material.anisotropyTextureUvScaleU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scaledMapLeft = pixelAt(8, 16);
    const auto scaledMapRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(scaledMapLeft[channel], horizontalDirectionPixel[channel],
                    0.04f)
            << "anisotropy texture U scale should repeat the first texel half, "
            << "left channel " << channel;
        EXPECT_NEAR(scaledMapRight[channel], horizontalDirectionPixel[channel],
                    0.04f)
            << "anisotropy texture U scale should repeat the first texel half, "
            << "right channel " << channel;
    }

    material.anisotropyTextureUvScaleU = 1.0f;
    material.anisotropyTextureUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto offsetMapLeft = pixelAt(8, 16);
    const auto offsetMapRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(offsetMapLeft[channel], verticalDirectionPixel[channel],
                    0.04f)
            << "anisotropy texture U offset should swap the left direction "
            << "sample, channel " << channel;
        EXPECT_NEAR(offsetMapRight[channel], horizontalDirectionPixel[channel],
                    0.04f)
            << "anisotropy texture U offset should swap the right direction "
            << "sample, channel " << channel;
    }

    material.anisotropy = 0.5f;
    material.anisotropyTexture = ArtifactCore::UniString(anisotropyMapPath);
    material.anisotropyTextureUvRotationDegrees = 0.0f;
    material.anisotropyTextureUvScaleU = 1.0f;
    material.anisotropyTextureUvOffsetU = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfScalarFullTexture = pixelAt(24, 16);

    QImage halfBlueAnisotropyMap = anisotropyMap;
    for (int y = 0; y < halfBlueAnisotropyMap.height(); ++y) {
        auto* row = halfBlueAnisotropyMap.scanLine(y);
        for (int x = halfBlueAnisotropyMap.width() / 2;
             x < halfBlueAnisotropyMap.width(); ++x) {
            row[x * 4 + 2] = 128;
        }
    }
    const QString halfBlueAnisotropyMapPath = textureDirectory.filePath(
        QStringLiteral("anisotropy-half-strength.png"));
    ASSERT_TRUE(halfBlueAnisotropyMap.save(
        halfBlueAnisotropyMapPath, "PNG"));
    material.anisotropy = 1.0f;
    material.anisotropyTexture =
        ArtifactCore::UniString(halfBlueAnisotropyMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfTextureBlue = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(halfTextureBlue[channel],
                    halfScalarFullTexture[channel], 0.035f)
            << "anisotropy factor should multiply the texture blue strength, "
               "channel " << channel;
    }

    QImage alphaChangedAnisotropyMap = halfBlueAnisotropyMap;
    for (int y = 0; y < alphaChangedAnisotropyMap.height(); ++y) {
        auto* row = alphaChangedAnisotropyMap.scanLine(y);
        for (int x = 0; x < alphaChangedAnisotropyMap.width(); ++x) {
            row[x * 4 + 3] = x < alphaChangedAnisotropyMap.width() / 2
                ? 0
                : 96;
        }
    }
    const QString alphaChangedAnisotropyMapPath = textureDirectory.filePath(
        QStringLiteral("anisotropy-alpha-changed.png"));
    ASSERT_TRUE(alphaChangedAnisotropyMap.save(
        alphaChangedAnisotropyMapPath, "PNG"));
    material.anisotropyTexture =
        ArtifactCore::UniString(alphaChangedAnisotropyMapPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-anisotropy-uv-set"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto alphaChangedPixel = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(alphaChangedPixel[channel], halfTextureBlue[channel], 0.02f)
            << "anisotropy alpha should not affect shading, channel "
            << channel;
    }
}

TEST_F(RayTracingAcceptanceTest, RotatesScalarAnisotropyInTangentSpace) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-scalar-anisotropy-rotation", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = 0.8f;
    material.baseColor[1] = 0.45f;
    material.baseColor[2] = 0.2f;
    material.metallic = 1.0f;
    material.roughness = 0.3f;
    material.anisotropy = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto isotropicPixel = pixelAt(16, 16);
    material.anisotropyRotationDegrees = 90.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedIsotropicPixel = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rotatedIsotropicPixel[channel], isotropicPixel[channel], 1.0e-5f)
            << "anisotropy rotation should not affect an isotropic material, channel "
            << channel;
    }

    material.anisotropy = 1.0f;
    material.anisotropyRotationDegrees = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto horizontalAnisotropyPixel = pixelAt(16, 16);

    material.anisotropyRotationDegrees = 90.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto verticalAnisotropyPixel = pixelAt(16, 16);
    const float anisotropyRotationDifference = std::max(
        std::fabs(horizontalAnisotropyPixel[0] - verticalAnisotropyPixel[0]),
        std::max(std::fabs(horizontalAnisotropyPixel[1] - verticalAnisotropyPixel[1]),
                 std::fabs(horizontalAnisotropyPixel[2] - verticalAnisotropyPixel[2])));
    EXPECT_GT(anisotropyRotationDifference, 0.001f)
        << "rotating scalar anisotropy should rotate the tangent-space highlight";

    const auto captureAnisotropy = [&](float anisotropy) {
        material.anisotropy = anisotropy;
        material.anisotropyRotationDegrees = 0.0f;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto minimumAnisotropyPixel = captureAnisotropy(-1.0f);
    const auto belowMinimumAnisotropyPixel = captureAnisotropy(-2.0f);
    const auto negativeAnisotropyPixel = captureAnisotropy(-0.75f);
    const auto zeroAnisotropyPixel = captureAnisotropy(0.0f);
    const auto positiveAnisotropyPixel = captureAnisotropy(0.75f);
    const auto maximumAnisotropyPixel = captureAnisotropy(1.0f);
    const auto aboveMaximumAnisotropyPixel = captureAnisotropy(2.0f);
    const auto nonFiniteAnisotropyPixel = captureAnisotropy(
        std::numeric_limits<float>::quiet_NaN());
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(zeroAnisotropyPixel[channel], isotropicPixel[channel],
                    1.0e-5f)
            << "zero anisotropy should match the isotropic baseline, channel "
            << channel;
        EXPECT_NEAR(belowMinimumAnisotropyPixel[channel],
                    minimumAnisotropyPixel[channel], 1.0e-5f)
            << "anisotropy below -1 should clamp to -1";
        EXPECT_NEAR(aboveMaximumAnisotropyPixel[channel],
                    maximumAnisotropyPixel[channel], 1.0e-5f)
            << "anisotropy above 1 should clamp to 1";
        EXPECT_NEAR(nonFiniteAnisotropyPixel[channel],
                    isotropicPixel[channel], 1.0e-5f)
            << "non-finite anisotropy should use its zero fallback";
    }
    const float anisotropySignDifference = std::max(
        std::fabs(negativeAnisotropyPixel[0] - positiveAnisotropyPixel[0]),
        std::max(std::fabs(negativeAnisotropyPixel[1] - positiveAnisotropyPixel[1]),
                 std::fabs(negativeAnisotropyPixel[2] - positiveAnisotropyPixel[2])));
    EXPECT_GT(anisotropySignDifference, 0.001f)
        << "positive and negative anisotropy should produce distinct tangent "
           "lobe responses";

    const auto captureSpecularTint = [&](std::size_t tintChannel, float tint) {
        material.anisotropy = 0.0f;
        material.metallic = 0.0f;
        material.ior = 1.5f;
        material.specularTint[0] = 1.0f;
        material.specularTint[1] = 1.0f;
        material.specularTint[2] = 1.0f;
        material.specularTint[tintChannel] = tint;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    for (std::size_t tintChannel = 0; tintChannel < 3; ++tintChannel) {
        const auto minimumSpecularTintPixel =
            captureSpecularTint(tintChannel, 0.0f);
        const auto belowMinimumSpecularTintPixel =
            captureSpecularTint(tintChannel, -0.5f);
        const auto maximumSpecularTintPixel =
            captureSpecularTint(tintChannel, 1.0f);
        const auto aboveMaximumSpecularTintPixel =
            captureSpecularTint(tintChannel, 1.5f);
        EXPECT_GT(std::fabs(maximumSpecularTintPixel[tintChannel] -
                            minimumSpecularTintPixel[tintChannel]),
                  0.001f)
            << "specular tint should affect its matching output channel "
            << tintChannel;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            if (channel != tintChannel) {
                EXPECT_NEAR(maximumSpecularTintPixel[channel],
                            minimumSpecularTintPixel[channel], 1.0e-5f)
                    << "specular tint channel " << tintChannel
                    << " should not alter output channel " << channel;
            }
            EXPECT_NEAR(belowMinimumSpecularTintPixel[channel],
                        minimumSpecularTintPixel[channel], 1.0e-5f)
                << "specular tint below zero should clamp to zero, tint channel "
                << tintChannel << ", output channel " << channel;
            EXPECT_NEAR(aboveMaximumSpecularTintPixel[channel],
                        maximumSpecularTintPixel[channel], 1.0e-5f)
                << "specular tint above one should clamp to one, tint channel "
                << tintChannel << ", output channel " << channel;
        }
    }

    QTemporaryDir normalMapDirectory;
    ASSERT_TRUE(normalMapDirectory.isValid());
    const auto tiltedNormalPath = normalMapDirectory.filePath(
        QStringLiteral("anisotropy-tilted-normal.png"));
    QImage tiltedNormal(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(tiltedNormal.isNull());
    tiltedNormal.fill(QColor(204, 128, 230, 255));
    ASSERT_TRUE(tiltedNormal.save(tiltedNormalPath, "PNG"));

    material.metallic = 1.0f;
    material.roughness = 0.3f;
    material.anisotropy = 0.75f;
    material.anisotropyRotationDegrees = 0.0f;
    material.specularTint[0] = 1.0f;
    material.specularTint[1] = 1.0f;
    material.specularTint[2] = 1.0f;
    material.normalTexture = ArtifactCore::UniString(tiltedNormalPath);
    material.normalStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto tiltedNormalHorizontalPixel = pixelAt(16, 16);

    material.anisotropyRotationDegrees = 90.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto tiltedNormalVerticalPixel = pixelAt(16, 16);
    const float normalMappedAnisotropyRotationDifference = std::max(
        std::fabs(tiltedNormalHorizontalPixel[0] - tiltedNormalVerticalPixel[0]),
        std::max(std::fabs(tiltedNormalHorizontalPixel[1] - tiltedNormalVerticalPixel[1]),
                 std::fabs(tiltedNormalHorizontalPixel[2] - tiltedNormalVerticalPixel[2])));
    EXPECT_GT(normalMappedAnisotropyRotationDifference, 0.001f)
        << "anisotropy rotation should remain effective with a normal map";

    material.normalStrength = 0.0f;
    material.anisotropyRotationDegrees = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-scalar-anisotropy-rotation"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto disabledNormalMapPixel = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(disabledNormalMapPixel[channel], positiveAnisotropyPixel[channel],
                    0.01f)
            << "zero normal strength should preserve scalar anisotropy shading, channel "
            << channel;
    }
}

TEST_F(RayTracingAcceptanceTest,
       UsesIorAndThinFilmForDielectricFresnelReflectance) {
    constexpr std::array<float, 9> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.0f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    constexpr std::array<float, 9> normals{
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-ior-fresnel", vertexBuffer.RawPtr(), 3,
                indexBuffer.RawPtr(), 3, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data(), normals.data());

    ArtifactCore::RTSceneLight normalIncidenceLight;
    normalIncidenceLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    normalIncidenceLight.directionIntensity[2] = 1.0f;
    normalIncidenceLight.directionIntensity[3] = 4.0f;
    normalIncidenceLight.colorAttenuationConstant[0] = 1.0f;
    normalIncidenceLight.colorAttenuationConstant[1] = 1.0f;
    normalIncidenceLight.colorAttenuationConstant[2] = 1.0f;
    normalIncidenceLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &normalIncidenceLight, 1);

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.0f;
    material.metallic = 0.0f;
    material.specular = 0.5f;
    material.roughness = 0.35f;
    const auto traceIor = [&](const float ior) {
        material.ior = ior;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-ior-fresnel"), material);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };

    const auto belowUnityIor = traceIor(0.5f);
    const auto unityIor = traceIor(1.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowUnityIor[channel], unityIor[channel], 1.0e-5f)
            << "IOR values below one should clamp to one, channel " << channel;
    }
    const auto glassIor = traceIor(1.5f);
    const auto highIor = traceIor(3.0f);
    EXPECT_GT(glassIor[0], unityIor[0] + 0.001f)
        << "dielectric Fresnel reflectance should increase above IOR one";
    EXPECT_GT(highIor[0], glassIor[0] + 0.001f)
        << "higher dielectric IOR should increase normal-incidence reflectance";

    const auto traceGrazingIor = [&](const float ior) {
        material.ior = ior;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-ior-fresnel"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ArtifactCore::RTRayCamera grazingCamera;
        grazingCamera.origin[0] = 2.0f;
        grazingCamera.origin[2] = -1.0f;
        grazingCamera.target[2] = 0.0f;
        grazingCamera.verticalFovDegrees = 60.0f;
        EXPECT_TRUE(manager_->traceCamera(
            context_.RawPtr(), kRayTraceTestImageSize,
            kRayTraceTestImageSize, grazingCamera));
        return pixelAt(kRayTraceTestImageSize / 2,
                       kRayTraceTestImageSize / 2);
    };
    const auto grazingUnityIor = traceGrazingIor(1.0f);
    const auto grazingGlassIor = traceGrazingIor(1.5f);
    const auto grazingHighIor = traceGrazingIor(3.0f);
    EXPECT_GT(grazingGlassIor[0], grazingUnityIor[0] + 0.001f)
        << "dielectric Fresnel should respond to IOR at grazing view angles";
    EXPECT_GT(grazingHighIor[0], grazingGlassIor[0] + 0.001f)
        << "grazing-view reflectance should rise with dielectric IOR";

    const auto traceThinFilm = [&](const float thicknessNm, const float filmIor) {
        material.ior = 1.5f;
        material.thinFilmThickness = thicknessNm;
        material.thinFilmIOR = filmIor;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-ior-fresnel"), material);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto thinFilmPixel = traceThinFilm(400.0f, 1.33f);
    EXPECT_GT(thinFilmPixel[1], thinFilmPixel[0] + 0.001f)
        << "thin-film interference should tint the dielectric reflection green";
    EXPECT_GT(thinFilmPixel[1], thinFilmPixel[2] + 0.001f)
        << "thin-film interference should separate wavelength-dependent reflectance";

    const auto expectSameRgb = [](const auto& lhs, const auto& rhs,
                                  const char* reason) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(lhs[channel], rhs[channel], 1.0e-5f)
                << reason << ", channel " << channel;
        }
    };
    expectSameRgb(traceThinFilm(0.0f, 1.33f), glassIor,
                  "zero film thickness should preserve dielectric Fresnel");
    expectSameRgb(traceThinFilm(0.0f, 1.33f),
                 traceThinFilm(-10.0f, 1.33f),
                 "negative film thickness should clamp to zero");
    expectSameRgb(traceThinFilm(1000.0f, 1.33f),
                 traceThinFilm(1200.0f, 1.33f),
                 "film thickness above 1000 nm should clamp to 1000 nm");
    expectSameRgb(traceThinFilm(400.0f, 1.0f),
                 traceThinFilm(400.0f, 0.5f),
                 "film IOR below one should clamp to one");
    expectSameRgb(traceThinFilm(400.0f, 2.5f),
                 traceThinFilm(400.0f, 3.0f),
                 "film IOR above 2.5 should clamp to 2.5");

    const auto traceTransmissiveThinFilm = [&](const float thicknessNm) {
        material.baseColor[0] = material.baseColor[1] =
            material.baseColor[2] = 1.0f;
        material.metallic = 0.0f;
        material.roughness = 0.2f;
        material.ior = 1.5f;
        material.transmission = 1.0f;
        material.thinFilmThickness = thicknessNm;
        material.thinFilmIOR = 1.33f;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-ior-fresnel"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(
            context_.RawPtr(), kRayTraceTestImageSize,
            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto noFilmTransmissionPixel = traceTransmissiveThinFilm(0.0f);
    const auto thinFilmTransmissionPixel = traceTransmissiveThinFilm(400.0f);
    float thinFilmTransmissionResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        thinFilmTransmissionResponse = std::max(
            thinFilmTransmissionResponse,
            std::fabs(thinFilmTransmissionPixel[channel] -
                      noFilmTransmissionPixel[channel]));
    }
    EXPECT_GT(thinFilmTransmissionResponse, 0.001f)
        << "thin-film Fresnel should affect the transmitted environment energy";

    const auto traceGrazingThinFilm = [&](const float thicknessNm) {
        material.ior = 1.5f;
        material.thinFilmThickness = thicknessNm;
        material.thinFilmIOR = 1.33f;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-ior-fresnel"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ArtifactCore::RTRayCamera grazingCamera;
        grazingCamera.origin[0] = 2.0f;
        grazingCamera.origin[2] = -1.0f;
        grazingCamera.target[2] = 0.0f;
        grazingCamera.verticalFovDegrees = 60.0f;
        EXPECT_TRUE(manager_->traceCamera(
            context_.RawPtr(), kRayTraceTestImageSize,
            kRayTraceTestImageSize, grazingCamera));
        return pixelAt(kRayTraceTestImageSize / 2,
                       kRayTraceTestImageSize / 2);
    };
    const auto grazingNoFilmPixel = traceGrazingThinFilm(0.0f);
    const auto grazingThinFilmPixel = traceGrazingThinFilm(400.0f);
    float grazingThinFilmResponse = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        grazingThinFilmResponse = std::max(
            grazingThinFilmResponse,
            std::fabs(grazingThinFilmPixel[channel] -
                      grazingNoFilmPixel[channel]));
    }
    EXPECT_GT(grazingThinFilmResponse, 0.001f)
        << "thin-film interference should affect dielectric reflection at a "
           "grazing view angle";
}

TEST_F(RayTracingAcceptanceTest,
       StopsTransmissionSkyForInternalViewAboveCriticalAngle) {
    constexpr std::array<float, 12> vertices{
        -4.0f, -4.0f, 0.0f, 4.0f, -4.0f, 0.0f,
         4.0f,  4.0f, 0.0f, -4.0f,  4.0f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 2, 1, 0, 3, 2};
    constexpr std::array<float, 12> outwardNormals{
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-internal-critical-angle", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data(), outwardNormals.data());

    ArtifactCore::RTSceneLight zeroAmbientLight;
    zeroAmbientLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Ambient);
    manager_->setSceneLights(context_.RawPtr(), &zeroAmbientLight, 1);

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 1.0f;
    material.metallic = 0.0f;
    material.roughness = 0.04f;
    material.transmission = 1.0f;
    material.thinWall = 0.0f;
    const auto traceInternalView = [&](
        const float ior, const std::array<float, 3>& target) {
        material.ior = ior;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-internal-critical-angle"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ArtifactCore::RTRayCamera camera;
        camera.origin[0] = 0.0f;
        camera.origin[1] = 0.0f;
        camera.origin[2] = 1.0f;
        camera.target[0] = target[0];
        camera.target[1] = target[1];
        camera.target[2] = target[2];
        camera.verticalFovDegrees = 0.1f;
        EXPECT_TRUE(manager_->traceCamera(
            context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize,
            camera));
        return pixelAt(16, 16);
    };

    // For IOR 1.5 the glass-to-air critical angle is about 41.81 degrees.
    // Bracket it with internal incidence at 40 and 43 degrees.
    const auto airReference = traceInternalView(
        1.0f, {0.6819984f, 0.0f, 0.2686463f});
    const auto subcriticalGlassView = traceInternalView(
        1.5f, {0.6427876f, 0.0f, 0.2339556f});
    const auto lowerIorInternalView = traceInternalView(
        1.33f, {0.6819984f, 0.0f, 0.2686463f});
    const auto glassInternalView = traceInternalView(
        1.5f, {0.6819984f, 0.0f, 0.2686463f});
    for (const auto& sample :
         std::array{airReference, subcriticalGlassView, lowerIorInternalView,
                    glassInternalView}) {
        for (std::size_t channel = 0; channel < sample.size(); ++channel) {
            EXPECT_TRUE(std::isfinite(sample[channel]))
                << "internal transmission sample channel " << channel
                << " should remain finite";
            EXPECT_GE(sample[channel], 0.0f)
                << "internal transmission sample channel " << channel
                << " should be non-negative radiance/alpha";
        }
    }
    const float airReferenceEnergy = airReference[0] + airReference[1] +
                                     airReference[2];
    const float subcriticalGlassEnergy = subcriticalGlassView[0] +
                                         subcriticalGlassView[1] +
                                         subcriticalGlassView[2];
    const float lowerIorInternalEnergy = lowerIorInternalView[0] +
                                         lowerIorInternalView[1] +
                                         lowerIorInternalView[2];
    const float glassInternalEnergy = glassInternalView[0] +
                                      glassInternalView[1] +
                                      glassInternalView[2];
    EXPECT_GT(airReferenceEnergy, 0.05f)
        << "the unity-IOR reference should transmit the fallback sky";
    EXPECT_GT(subcriticalGlassEnergy, 0.05f)
        << "an internal view below the critical angle should transmit the sky";
    EXPECT_GT(lowerIorInternalEnergy, 0.05f)
        << "43 degrees should remain below the critical angle for IOR 1.33";
    EXPECT_LT(glassInternalEnergy, subcriticalGlassEnergy * 0.1f)
        << "an above-critical internal view should not transmit the exterior sky";
    EXPECT_LT(glassInternalEnergy, lowerIorInternalEnergy * 0.1f)
        << "the same internal angle should become totally reflecting as IOR "
           "increases above its critical-angle threshold";
}

TEST_F(RayTracingAcceptanceTest, AppliesThinWallTransmissionFresnelAdjustment) {
    constexpr std::array<float, 9> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.0f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    constexpr std::array<float, 9> normals{
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-thin-wall-transmission", vertexBuffer.RawPtr(), 3,
                indexBuffer.RawPtr(), 3, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data(), normals.data());

    ArtifactCore::RTSceneLight zeroAmbientLight;
    zeroAmbientLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Ambient);
    manager_->setSceneLights(context_.RawPtr(), &zeroAmbientLight, 1);

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 1.0f;
    material.metallic = 0.0f;
    material.specular = 0.5f;
    material.roughness = 0.2f;
    material.transmission = 1.0f;
    const auto traceThinWall = [&](const float ior, const float thinWall) {
        material.ior = ior;
        material.thinWall = thinWall;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-thin-wall-transmission"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };

    const auto unityIorThick = traceThinWall(1.0f, 0.0f);
    const auto unityIorThin = traceThinWall(1.0f, 1.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unityIorThin[channel], unityIorThick[channel], 1.0e-5f)
            << "thin-wall mode should match the solid path at IOR one, channel "
            << channel;
    }

    const auto glassIorThick = traceThinWall(1.5f, 0.0f);
    const auto glassIorThin = traceThinWall(1.5f, 1.0f);
    const float thinWallDifference = std::max(
        std::fabs(glassIorThin[0] - glassIorThick[0]),
        std::max(std::fabs(glassIorThin[1] - glassIorThick[1]),
                 std::fabs(glassIorThin[2] - glassIorThick[2])));
    EXPECT_GT(thinWallDifference, 0.001f)
        << "thin-wall transmission should apply its adjusted dielectric Fresnel";
    const auto rgbEnergy = [](const std::array<float, 4>& pixel) {
        return pixel[0] + pixel[1] + pixel[2];
    };
    EXPECT_GT(rgbEnergy(glassIorThick), rgbEnergy(glassIorThin) + 0.001f)
        << "thin-wall Fresnel should reduce transmitted energy at normal view";

    const auto traceGrazingThinWall = [&](const float thinWall) {
        material.ior = 1.5f;
        material.thinWall = thinWall;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-thin-wall-transmission"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ArtifactCore::RTRayCamera grazingCamera;
        grazingCamera.origin[0] = 2.0f;
        grazingCamera.origin[2] = -1.0f;
        grazingCamera.target[2] = 0.0f;
        grazingCamera.verticalFovDegrees = 60.0f;
        EXPECT_TRUE(manager_->traceCamera(
            context_.RawPtr(), kRayTraceTestImageSize,
            kRayTraceTestImageSize, grazingCamera));
        return pixelAt(kRayTraceTestImageSize / 2,
                       kRayTraceTestImageSize / 2);
    };
    const auto grazingThick = traceGrazingThinWall(0.0f);
    const auto grazingThin = traceGrazingThinWall(1.0f);
    const float grazingThinWallDifference = std::max(
        std::fabs(grazingThin[0] - grazingThick[0]),
        std::max(std::fabs(grazingThin[1] - grazingThick[1]),
                 std::fabs(grazingThin[2] - grazingThick[2])));
    EXPECT_GT(grazingThinWallDifference, 0.001f)
        << "thin-wall transmission should adjust Fresnel at grazing view angles";
    EXPECT_GT(rgbEnergy(grazingThick), rgbEnergy(grazingThin) + 0.001f)
        << "thin-wall Fresnel should reduce transmitted energy at grazing view";

    const auto belowMinimumThinWall = traceThinWall(1.5f, -0.5f);
    const auto nonFiniteThinWall = traceThinWall(
        1.5f, std::numeric_limits<float>::quiet_NaN());
    const auto aboveMaximumThinWall = traceThinWall(1.5f, 1.5f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumThinWall[channel], glassIorThick[channel],
                    1.0e-5f)
            << "thin-wall amount below zero should clamp to the solid path, channel "
            << channel;
        EXPECT_NEAR(nonFiniteThinWall[channel], glassIorThick[channel],
                    1.0e-5f)
            << "non-finite thin-wall amount should use the zero fallback, channel "
            << channel;
        EXPECT_NEAR(aboveMaximumThinWall[channel], glassIorThin[channel],
                    1.0e-5f)
            << "thin-wall amount above one should clamp to the thin path, channel "
            << channel;
    }

    const auto traceTransmission = [&](const float transmission) {
        material.ior = 1.5f;
        material.thinWall = 0.0f;
        material.transmission = transmission;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-thin-wall-transmission"), material);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto zeroTransmission = traceTransmission(0.0f);
    const auto negativeTransmission = traceTransmission(-0.5f);
    const auto fullTransmission = traceTransmission(1.0f);
    const auto excessTransmission = traceTransmission(1.5f);
    const auto nonFiniteTransmission = traceTransmission(
        std::numeric_limits<float>::quiet_NaN());
    const auto infiniteTransmission = traceTransmission(
        std::numeric_limits<float>::infinity());
    const float transmissionResponse = std::max(
        std::fabs(fullTransmission[0] - zeroTransmission[0]),
        std::max(std::fabs(fullTransmission[1] - zeroTransmission[1]),
                 std::fabs(fullTransmission[2] - zeroTransmission[2])));
    EXPECT_GT(transmissionResponse, 0.001f)
        << "the fixture should distinguish zero from full transmission";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(negativeTransmission[channel], zeroTransmission[channel],
                    1.0e-5f)
            << "negative transmission should clamp to zero, channel " << channel;
        EXPECT_NEAR(excessTransmission[channel], fullTransmission[channel],
                    1.0e-5f)
            << "transmission above one should clamp to one, channel " << channel;
        EXPECT_NEAR(nonFiniteTransmission[channel], zeroTransmission[channel],
                    1.0e-5f)
            << "non-finite transmission should use its zero fallback, channel "
            << channel;
        EXPECT_NEAR(infiniteTransmission[channel], zeroTransmission[channel],
                    1.0e-5f)
            << "infinite transmission should use its zero fallback, channel "
            << channel;
    }

    constexpr std::array<float, 9> reversedNormals{
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-thin-wall-transmission"), vertices.data(),
        3, indices.data(), 3, reversedNormals.data()));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto reversedNormalTransmission = pixelAt(16, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(reversedNormalTransmission[channel], fullTransmission[channel],
                    1.0e-5f)
            << "face-forward shading should preserve transmission for reversed "
               "vertex normals, channel " << channel;
    }

    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-thin-wall-transmission"),
        vertices.data(), 3, indices.data(), 3, normals.data()));
    material.clearcoat = 1.0f;
    material.clearcoatRoughness = 0.2f;
    material.sheen = 1.0f;
    material.sheenRoughness = 0.25f;
    material.transmission = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-thin-wall-transmission"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto layeredFullTransmission = pixelAt(16, 16);

    material.transmission = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-thin-wall-transmission"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto layeredZeroTransmission = pixelAt(16, 16);
    float layeredTransmissionResponse = 0.0f;
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_TRUE(std::isfinite(layeredFullTransmission[channel]))
            << "layered transmission should remain finite, channel " << channel;
        EXPECT_GE(layeredFullTransmission[channel], 0.0f)
            << "layered transmission should remain non-negative, channel "
            << channel;
        layeredTransmissionResponse = std::max(
            layeredTransmissionResponse,
            std::fabs(layeredFullTransmission[channel] -
                      layeredZeroTransmission[channel]));
    }
    EXPECT_GT(layeredTransmissionResponse, 0.001f)
        << "transmission should contribute alongside clearcoat and sheen";

    ArtifactCore::RTSceneLight greenAreaBacklight;
    greenAreaBacklight.positionType[2] = 1.0f;
    greenAreaBacklight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Area);
    greenAreaBacklight.directionIntensity[2] = -1.0f;
    greenAreaBacklight.directionIntensity[3] = 32.0f;
    greenAreaBacklight.colorAttenuationConstant[1] = 1.0f;
    greenAreaBacklight.colorAttenuationConstant[3] = 1.0f;
    greenAreaBacklight.shadowParams[0] = -1.0f;
    greenAreaBacklight.shadowParams[1] = 0.8f;
    greenAreaBacklight.shadowParams[2] = 0.8f;
    greenAreaBacklight.shadowParams[3] = 0.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenAreaBacklight, 1);
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 1.0f;
    material.metallic = 0.0f;
    material.specular = 0.0f;
    material.clearcoat = 0.0f;
    material.sheen = 0.0f;
    material.thinWall = 1.0f;
    const auto traceAreaLightTransmission = [&](const float transmission) {
        material.transmission = transmission;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-thin-wall-transmission"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(
            context_.RawPtr(), kRayTraceTestImageSize,
            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto opaqueAreaBacklightPixel = traceAreaLightTransmission(0.0f);
    const auto transmittedAreaBacklightPixel = traceAreaLightTransmission(1.0f);
    EXPECT_GT(transmittedAreaBacklightPixel[1],
              opaqueAreaBacklightPixel[1] + 0.001f)
        << "a green area backlight should contribute through a transmissive "
           "thin-wall material";

    constexpr std::array<float, 12> areaShadowBlockerVertices{
        0.08f, -0.06f, 0.5f, 0.12f, -0.06f, 0.5f,
        0.12f,  0.06f, 0.5f, 0.08f,  0.06f, 0.5f};
    constexpr std::array<std::uint32_t, 6> areaShadowBlockerIndices{
        0, 1, 2, 0, 2, 3};
    auto areaShadowBlockerVertexBuffer = makeBuffer(
        areaShadowBlockerVertices.data(), sizeof(areaShadowBlockerVertices),
        3u * sizeof(float), BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE,
        BUFFER_MODE_STRUCTURED);
    auto areaShadowBlockerIndexBuffer = makeBuffer(
        areaShadowBlockerIndices.data(), sizeof(areaShadowBlockerIndices),
        sizeof(std::uint32_t), BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE,
        BUFFER_MODE_STRUCTURED);
    ASSERT_NE(areaShadowBlockerVertexBuffer, nullptr);
    ASSERT_NE(areaShadowBlockerIndexBuffer, nullptr);
    addGeometry("pbr-transmission-area-shadow-blocker",
                areaShadowBlockerVertexBuffer.RawPtr(), 4,
                areaShadowBlockerIndexBuffer.RawPtr(), 6,
                float4x4::Identity(), 1, 1,
                areaShadowBlockerVertices.data(),
                areaShadowBlockerIndices.data());
    ArtifactCore::RTPbrMaterial areaShadowBlockerMaterial;
    areaShadowBlockerMaterial.baseColor[0] =
        areaShadowBlockerMaterial.baseColor[1] =
            areaShadowBlockerMaterial.baseColor[2] = 0.0f;
    areaShadowBlockerMaterial.metallic = 0.0f;
    areaShadowBlockerMaterial.specular = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-transmission-area-shadow-blocker"),
        areaShadowBlockerMaterial));

    greenAreaBacklight.positionType[0] = 0.2f;
    greenAreaBacklight.shadowParams[1] = 0.1f;
    greenAreaBacklight.shadowParams[2] = 0.1f;
    const auto traceAreaBacklightShadow = [&](const float shadowSamples) {
        greenAreaBacklight.shadowParams[0] = shadowSamples;
        manager_->setSceneLights(context_.RawPtr(), &greenAreaBacklight, 1);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(
            context_.RawPtr(), kRayTraceTestImageSize,
            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };
    const auto unshadowedAreaBacklightPixel = traceAreaBacklightShadow(-1.0f);
    const auto partiallyShadowedAreaBacklightPixel =
        traceAreaBacklightShadow(0.0f);
    const auto filteredAreaBacklightShadowPixel =
        traceAreaBacklightShadow(2.0f);
    EXPECT_GT(unshadowedAreaBacklightPixel[1],
              partiallyShadowedAreaBacklightPixel[1] + 0.001f)
        << "an opaque blocker should reduce area-light transmission while the "
           "straight-through camera ray misses it";
    EXPECT_GT(std::fabs(filteredAreaBacklightShadowPixel[1] -
                        partiallyShadowedAreaBacklightPixel[1]),
              0.001f)
        << "area-light transmission should respond to its shadow sampling "
           "setting";
}

TEST_F(RayTracingAcceptanceTest, SelectsTransmissionPerMaterialSlot) {
    constexpr std::array<float, 18> vertices{
        -0.9f, -0.9f, 0.0f, 0.0f, -0.9f, 0.0f, -0.45f, 0.9f, 0.0f,
         0.0f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.45f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
    constexpr std::array<std::uint32_t, 2> materialIndices{0, 1};
    constexpr std::array<float, 18> normals{
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("transmission-material-slots", vertexBuffer.RawPtr(), 6,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data(), normals.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("transmission-material-slots"), vertices.data(),
        6, indices.data(), 6, normals.data(), materialIndices.data()));

    ArtifactCore::RTSceneLight zeroAmbientLight;
    zeroAmbientLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Ambient);
    manager_->setSceneLights(context_.RawPtr(), &zeroAmbientLight, 1);

    ArtifactCore::RTPbrMaterial leftMaterial;
    leftMaterial.baseColor[0] = leftMaterial.baseColor[1] =
        leftMaterial.baseColor[2] = 1.0f;
    leftMaterial.metallic = 0.0f;
    leftMaterial.specular = 0.5f;
    leftMaterial.roughness = 0.2f;
    leftMaterial.ior = 1.5f;
    leftMaterial.transmission = 0.0f;
    ArtifactCore::RTPbrMaterial rightMaterial = leftMaterial;
    rightMaterial.transmission = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), rightMaterial,
        1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto zeroTransmissionLeftPixel = pixelAt(8, 16);
    const auto fullTransmissionRightPixel = pixelAt(24, 16);

    leftMaterial.transmission = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto fullTransmissionLeftPixel = pixelAt(8, 16);
    const auto unchangedFullTransmissionRightPixel = pixelAt(24, 16);
    const float leftTransmissionDifference = std::max(
        std::fabs(fullTransmissionLeftPixel[0] - zeroTransmissionLeftPixel[0]),
        std::max(std::fabs(fullTransmissionLeftPixel[1] -
                           zeroTransmissionLeftPixel[1]),
                 std::fabs(fullTransmissionLeftPixel[2] -
                           zeroTransmissionLeftPixel[2])));
    EXPECT_GT(leftTransmissionDifference, 0.001f)
        << "slot 0 transmission should change its shading response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedFullTransmissionRightPixel[channel],
                    fullTransmissionRightPixel[channel], 0.01f)
            << "changing slot 0 transmission should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.transmission = 0.0f;
    rightMaterial.transmission = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), rightMaterial,
        1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedZeroTransmissionLeftPixel = pixelAt(8, 16);
    const auto zeroTransmissionRightPixel = pixelAt(24, 16);
    const float rightTransmissionDifference = std::max(
        std::fabs(zeroTransmissionRightPixel[0] -
                  fullTransmissionRightPixel[0]),
        std::max(std::fabs(zeroTransmissionRightPixel[1] -
                           fullTransmissionRightPixel[1]),
                 std::fabs(zeroTransmissionRightPixel[2] -
                           fullTransmissionRightPixel[2])));
    EXPECT_GT(rightTransmissionDifference, 0.001f)
        << "slot 1 transmission should change its shading response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedZeroTransmissionLeftPixel[channel],
                    zeroTransmissionLeftPixel[channel], 0.01f)
            << "changing slot 1 transmission should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.transmission = rightMaterial.transmission = 1.0f;
    leftMaterial.ior = rightMaterial.ior = 1.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), rightMaterial,
        1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baselineLeftIorPixel = pixelAt(8, 16);
    const auto baselineRightIorPixel = pixelAt(24, 16);

    leftMaterial.ior = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unityIorLeftPixel = pixelAt(8, 16);
    const auto unchangedRightIorPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(unityIorLeftPixel[0] - baselineLeftIorPixel[0]),
              0.001f)
        << "slot 0 IOR should change its dielectric Fresnel response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedRightIorPixel[channel],
                    baselineRightIorPixel[channel], 0.01f)
            << "changing slot 0 IOR should preserve slot 1, channel "
            << channel;
    }

    leftMaterial.ior = 1.5f;
    rightMaterial.ior = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), rightMaterial,
        1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unchangedLeftIorPixel = pixelAt(8, 16);
    const auto highIorRightPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(highIorRightPixel[0] - baselineRightIorPixel[0]),
              0.001f)
        << "slot 1 IOR should change its dielectric Fresnel response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(unchangedLeftIorPixel[channel], baselineLeftIorPixel[channel],
                    0.01f)
            << "changing slot 1 IOR should preserve slot 0, channel "
            << channel;
    }

    leftMaterial.ior = 1.0f;
    leftMaterial.transmission = 1.0f;
    rightMaterial.ior = 3.0f;
    rightMaterial.transmission = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), rightMaterial,
        1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto mixedLeftPixel = pixelAt(8, 16);
    const auto mixedRightPixel = pixelAt(24, 16);

    leftMaterial.ior = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), leftMaterial,
        0));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto highFresnelLeftPixel = pixelAt(8, 16);
    const auto preservedOpaqueRightPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(highFresnelLeftPixel[0] - mixedLeftPixel[0]), 0.001f)
        << "slot 0 IOR should affect the transmissive slot's Fresnel response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(preservedOpaqueRightPixel[channel], mixedRightPixel[channel],
                    0.01f)
            << "changing slot 0 IOR should preserve opaque slot 1, channel "
            << channel;
    }

    rightMaterial.transmission = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("transmission-material-slots"), rightMaterial,
        1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto leftAfterRightTransmission = pixelAt(8, 16);
    const auto highIorTransmissionRightPixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(highIorTransmissionRightPixel[0] - mixedRightPixel[0]),
              0.001f)
        << "transmission should activate through slot 1's high-IOR Fresnel";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(leftAfterRightTransmission[channel], highFresnelLeftPixel[channel],
                    0.01f)
            << "changing slot 1 transmission should preserve slot 0, channel "
            << channel;
    }
}

TEST_F(RayTracingAcceptanceTest,
       AppliesDiffuseRoughnessToBurleyDiffuseLighting) {
    constexpr std::array<float, 9> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.0f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    constexpr std::array<float, 9> normals{
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-diffuse-roughness", vertexBuffer.RawPtr(), 3,
                indexBuffer.RawPtr(), 3, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data(), normals.data());

    ArtifactCore::RTSceneLight obliqueLight;
    obliqueLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    obliqueLight.directionIntensity[0] = 0.6f;
    obliqueLight.directionIntensity[2] = 0.8f;
    obliqueLight.directionIntensity[3] = 4.0f;
    obliqueLight.colorAttenuationConstant[0] = 1.0f;
    obliqueLight.colorAttenuationConstant[1] = 1.0f;
    obliqueLight.colorAttenuationConstant[2] = 1.0f;
    obliqueLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &obliqueLight, 1);

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.7f;
    material.metallic = 0.0f;
    material.specular = 0.0f;
    material.ior = 1.0f;
    const auto traceDiffuseRoughness = [&](float diffuseRoughness) {
        material.diffuseRoughness = diffuseRoughness;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-diffuse-roughness"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };

    const auto smoothDiffusePixel = traceDiffuseRoughness(0.0f);
    const auto roughDiffusePixel = traceDiffuseRoughness(1.0f);
    const float diffuseRoughnessResponse = std::max(
        std::fabs(roughDiffusePixel[0] - smoothDiffusePixel[0]),
        std::max(std::fabs(roughDiffusePixel[1] - smoothDiffusePixel[1]),
                 std::fabs(roughDiffusePixel[2] - smoothDiffusePixel[2])));
    EXPECT_GT(diffuseRoughnessResponse, 0.001f)
        << "diffuse roughness should change the direct Oren-Nayar response";

    const auto belowMinimumDiffuseRoughness = traceDiffuseRoughness(-0.5f);
    const auto aboveMaximumDiffuseRoughness = traceDiffuseRoughness(1.5f);
    const auto nonFiniteDiffuseRoughness = traceDiffuseRoughness(
        std::numeric_limits<float>::quiet_NaN());
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumDiffuseRoughness[channel],
                    smoothDiffusePixel[channel], 1.0e-5f)
            << "diffuse roughness below zero should clamp to zero, channel "
            << channel;
        EXPECT_NEAR(aboveMaximumDiffuseRoughness[channel],
                    roughDiffusePixel[channel], 1.0e-5f)
            << "diffuse roughness above one should clamp to one, channel "
            << channel;
        EXPECT_NEAR(nonFiniteDiffuseRoughness[channel],
                    smoothDiffusePixel[channel], 1.0e-5f)
            << "non-finite diffuse roughness should use its zero fallback, channel "
            << channel;
    }
}

TEST_F(RayTracingAcceptanceTest,
       AppliesSubsurfaceAmountAndRgbRadiusToDiffuseLighting) {
    constexpr std::array<float, 9> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.0f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    constexpr std::array<float, 9> normals{
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-subsurface-diffuse", vertexBuffer.RawPtr(), 3,
                indexBuffer.RawPtr(), 3, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data(), normals.data());

    ArtifactCore::RTSceneLight normalIncidenceLight;
    normalIncidenceLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    normalIncidenceLight.directionIntensity[2] = 1.0f;
    normalIncidenceLight.directionIntensity[3] = 4.0f;
    normalIncidenceLight.colorAttenuationConstant[0] = 1.0f;
    normalIncidenceLight.colorAttenuationConstant[1] = 1.0f;
    normalIncidenceLight.colorAttenuationConstant[2] = 1.0f;
    normalIncidenceLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &normalIncidenceLight, 1);

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 1.0f;
    material.metallic = 0.0f;
    material.specular = 0.0f;
    material.roughness = 0.5f;
    const auto traceSubsurface = [&](const float amount) {
        material.subsurface = amount;
        manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-subsurface-diffuse"), material);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16);
    };

    const auto burleyDiffusePixel = traceSubsurface(0.0f);
    const auto mixedDiffusePixel = traceSubsurface(0.5f);
    const auto subsurfaceDiffusePixel = traceSubsurface(1.0f);
    const auto aboveMaximumSubsurfacePixel = traceSubsurface(1.5f);
    const auto negativeSubsurfacePixel = traceSubsurface(-0.5f);
    const auto nonFiniteSubsurfacePixel = traceSubsurface(
        std::numeric_limits<float>::quiet_NaN());
    EXPECT_GT(subsurfaceDiffusePixel[0], subsurfaceDiffusePixel[1] + 0.01f)
        << "the red subsurface radius should exceed the green radius response";
    EXPECT_GT(subsurfaceDiffusePixel[1], subsurfaceDiffusePixel[2] + 0.01f)
        << "the green subsurface radius should exceed the blue radius response";
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(aboveMaximumSubsurfacePixel[channel],
                    subsurfaceDiffusePixel[channel], 1.0e-5f)
            << "subsurface amount above one should clamp to one, channel "
            << channel;
        EXPECT_NEAR(negativeSubsurfacePixel[channel],
                    burleyDiffusePixel[channel], 1.0e-5f)
            << "negative subsurface amount should clamp to zero, channel "
            << channel;
        EXPECT_NEAR(nonFiniteSubsurfacePixel[channel],
                    burleyDiffusePixel[channel], 1.0e-5f)
            << "non-finite subsurface amount should use the zero fallback, channel "
            << channel;
        EXPECT_GT(mixedDiffusePixel[channel],
                  subsurfaceDiffusePixel[channel] + 0.001f)
            << "a partial subsurface amount should blend from Burley diffuse, channel "
            << channel;
        EXPECT_LT(mixedDiffusePixel[channel], burleyDiffusePixel[channel] - 0.001f)
            << "a partial subsurface amount should remain below Burley diffuse, channel "
            << channel;
    }

    constexpr std::array<float, 3> defaultSubsurfaceRadius{1.0f, 0.2f, 0.1f};
    for (std::size_t channel = 0; channel < 3; ++channel) {
        material.subsurfaceRadius[channel] = 1.0f;
        material.subsurfaceColor[channel] = 1.0f;
    }
    const auto neutralSubsurfaceColorPixel = traceSubsurface(1.0f);
    std::array<std::array<float, 4>, 3> zeroSubsurfaceColorPixels{};
    for (std::size_t tintChannel = 0; tintChannel < 3; ++tintChannel) {
        material.subsurfaceColor[0] = 1.0f;
        material.subsurfaceColor[1] = 1.0f;
        material.subsurfaceColor[2] = 1.0f;
        material.subsurfaceColor[tintChannel] = 0.0f;
        const auto tintedSubsurfacePixel = traceSubsurface(1.0f);
        zeroSubsurfaceColorPixels[tintChannel] = tintedSubsurfacePixel;
        EXPECT_LT(tintedSubsurfacePixel[tintChannel],
                  neutralSubsurfaceColorPixel[tintChannel] - 0.001f)
            << "subsurface color should reduce its matching output channel "
            << tintChannel;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            if (channel != tintChannel) {
                EXPECT_NEAR(tintedSubsurfacePixel[channel],
                            neutralSubsurfaceColorPixel[channel], 1.0e-5f)
                    << "subsurface color channel " << tintChannel
                    << " should not alter output channel " << channel;
            }
        }
    }
    for (std::size_t tintChannel = 0; tintChannel < 3; ++tintChannel) {
        material.subsurfaceColor[0] = 1.0f;
        material.subsurfaceColor[1] = 1.0f;
        material.subsurfaceColor[2] = 1.0f;
        material.subsurfaceColor[tintChannel] = -0.5f;
        const auto belowMinimumSubsurfaceColorPixel = traceSubsurface(1.0f);
        material.subsurfaceColor[tintChannel] = 1.5f;
        const auto aboveMaximumSubsurfaceColorPixel = traceSubsurface(1.0f);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(belowMinimumSubsurfaceColorPixel[channel],
                        zeroSubsurfaceColorPixels[tintChannel][channel],
                        1.0e-5f)
                << "subsurface color channel " << tintChannel
                << " below zero should clamp, output channel " << channel;
            EXPECT_NEAR(aboveMaximumSubsurfaceColorPixel[channel],
                        neutralSubsurfaceColorPixel[channel], 1.0e-5f)
                << "subsurface color channel " << tintChannel
                << " above one should clamp, output channel " << channel;
        }
    }
    for (std::size_t invalidChannel = 0; invalidChannel < 3; ++invalidChannel) {
        material.subsurfaceColor[0] = 1.0f;
        material.subsurfaceColor[1] = 1.0f;
        material.subsurfaceColor[2] = 1.0f;
        material.subsurfaceColor[invalidChannel] =
            std::numeric_limits<float>::quiet_NaN();
        const auto nonFiniteSubsurfaceColorPixel = traceSubsurface(1.0f);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(nonFiniteSubsurfaceColorPixel[channel],
                        neutralSubsurfaceColorPixel[channel], 1.0e-5f)
                << "non-finite subsurface color channel " << invalidChannel
                << " should use its white fallback without changing channel "
                << channel;
        }
    }
    for (std::size_t channel = 0; channel < 3; ++channel) {
        material.subsurfaceColor[channel] = 1.0f;
        material.subsurfaceRadius[channel] = defaultSubsurfaceRadius[channel];
    }

    material.subsurfaceAnisotropy = -0.8f;
    const auto backwardScatterPixel = traceSubsurface(1.0f);
    material.subsurfaceAnisotropy = 0.8f;
    const auto forwardScatterPixel = traceSubsurface(1.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_GT(backwardScatterPixel[channel],
                  subsurfaceDiffusePixel[channel])
            << "negative subsurface anisotropy should strengthen backscatter, channel "
            << channel;
        EXPECT_GT(subsurfaceDiffusePixel[channel],
                  forwardScatterPixel[channel])
            << "positive subsurface anisotropy should suppress backscatter, channel "
            << channel;
    }

    material.subsurfaceAnisotropy = 0.0f;
    material.subsurfaceIOR = 0.5f;
    const auto belowMinimumSubsurfaceIor = traceSubsurface(1.0f);
    material.subsurfaceIOR = 1.0f;
    const auto minimumSubsurfaceIor = traceSubsurface(1.0f);
    EXPECT_NEAR(belowMinimumSubsurfaceIor[0], minimumSubsurfaceIor[0], 1.0e-5f)
        << "subsurface IOR below one should clamp to one";
    material.subsurfaceIOR = 2.0f;
    const auto maximumSubsurfaceIor = traceSubsurface(1.0f);
    const float subsurfaceIorResponse = std::max(
        std::fabs(maximumSubsurfaceIor[0] - minimumSubsurfaceIor[0]),
        std::max(std::fabs(maximumSubsurfaceIor[1] - minimumSubsurfaceIor[1]),
                 std::fabs(maximumSubsurfaceIor[2] - minimumSubsurfaceIor[2])));
    EXPECT_GT(subsurfaceIorResponse, 0.001f)
        << "subsurface IOR should change the diffuse Fresnel response";
    material.subsurfaceIOR = 2.5f;
    const auto aboveMaximumSubsurfaceIor = traceSubsurface(1.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(aboveMaximumSubsurfaceIor[channel],
                    maximumSubsurfaceIor[channel], 1.0e-5f)
            << "subsurface IOR above two should clamp to two, channel "
            << channel;
    }
    material.subsurfaceIOR = std::numeric_limits<float>::quiet_NaN();
    const auto nonFiniteSubsurfaceIor = traceSubsurface(1.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(nonFiniteSubsurfaceIor[channel],
                    subsurfaceDiffusePixel[channel], 1.0e-5f)
            << "non-finite subsurface IOR should use 1.4, channel "
            << channel;
    }

    material.subsurfaceIOR = 1.4f;
    material.subsurfaceAnisotropy = -0.9f;
    const auto minimumNegativeSubsurfaceAnisotropy = traceSubsurface(1.0f);
    material.subsurfaceAnisotropy = -2.0f;
    const auto belowMinimumSubsurfaceAnisotropy = traceSubsurface(1.0f);
    material.subsurfaceAnisotropy = std::numeric_limits<float>::quiet_NaN();
    const auto nonFiniteSubsurfaceAnisotropy = traceSubsurface(1.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumSubsurfaceAnisotropy[channel],
                    minimumNegativeSubsurfaceAnisotropy[channel], 1.0e-5f)
            << "subsurface anisotropy below -0.9 should clamp, channel "
            << channel;
        EXPECT_NEAR(nonFiniteSubsurfaceAnisotropy[channel],
                    subsurfaceDiffusePixel[channel], 1.0e-5f)
            << "non-finite subsurface anisotropy should use zero, channel "
            << channel;
    }
    material.subsurfaceAnisotropy = 0.9f;
    const auto maximumSubsurfaceAnisotropy = traceSubsurface(1.0f);
    material.subsurfaceAnisotropy = 2.0f;
    const auto overMaximumSubsurfaceAnisotropy = traceSubsurface(1.0f);
    EXPECT_NEAR(overMaximumSubsurfaceAnisotropy[0],
                maximumSubsurfaceAnisotropy[0], 1.0e-5f)
        << "subsurface anisotropy above 0.9 should clamp to 0.9";

    material.subsurfaceAnisotropy = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        material.subsurfaceRadius[channel] = 0.0f;
        const auto minimumSubsurfaceRadius = traceSubsurface(1.0f);
        material.subsurfaceRadius[channel] = -1.0f;
        const auto belowMinimumSubsurfaceRadius = traceSubsurface(1.0f);
        material.subsurfaceRadius[channel] =
            std::numeric_limits<float>::quiet_NaN();
        const auto nonFiniteSubsurfaceRadius = traceSubsurface(1.0f);
        EXPECT_NEAR(belowMinimumSubsurfaceRadius[channel],
                    minimumSubsurfaceRadius[channel], 1.0e-5f)
            << "subsurface radius below zero should clamp, channel "
            << channel;
        EXPECT_NEAR(nonFiniteSubsurfaceRadius[channel],
                    subsurfaceDiffusePixel[channel], 1.0e-5f)
            << "non-finite subsurface radius should use its channel default, channel "
            << channel;
        material.subsurfaceRadius[channel] = defaultSubsurfaceRadius[channel];
    }
    material.subsurfaceRadius[0] = 10.0f;
    const auto maximumSubsurfaceRadius = traceSubsurface(1.0f);
    material.subsurfaceRadius[0] = 20.0f;
    const auto overMaximumSubsurfaceRadius = traceSubsurface(1.0f);
    EXPECT_NEAR(overMaximumSubsurfaceRadius[0],
                maximumSubsurfaceRadius[0], 1.0e-5f)
        << "subsurface radius above ten should clamp to ten";
}

TEST_F(RayTracingAcceptanceTest, MaskedOpacityTextureContinuesRayToRearSurface) {
    constexpr std::array<float, 24> vertices{
        -0.9f, -0.9f,  0.4f, 0.9f, -0.9f,  0.4f,
         0.9f,  0.9f,  0.4f, -0.9f, 0.9f,  0.4f,
        -0.7f, -0.7f, -0.4f, 0.7f, -0.7f, -0.4f,
         0.7f,  0.7f, -0.4f, -0.7f, 0.7f, -0.4f};
    constexpr std::array<std::uint32_t, 12> indices{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    constexpr std::array<std::uint32_t, 4> materialIndices{0, 0, 1, 1};
    constexpr std::array<float, 16> uvs{
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 16> mirroredUvs1{
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("masked-alpha-surfaces", vertexBuffer.RawPtr(), 8,
                indexBuffer.RawPtr(), 12, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("masked-alpha-surfaces"), vertices.data(), 8,
        indices.data(), 12, nullptr, materialIndices.data(), uvs.data(),
        nullptr, mirroredUvs1.data()));

    QTemporaryDir maskedOpacityDirectory;
    ASSERT_TRUE(maskedOpacityDirectory.isValid());
    const auto texturePath = std::filesystem::path(
        maskedOpacityDirectory.filePath(QStringLiteral("masked-opacity.png"))
            .toStdString());
    QImage opacityImage(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(opacityImage.isNull());
    for (int y = 0; y < opacityImage.height(); ++y) {
        auto* row = opacityImage.scanLine(y);
        for (int x = 0; x < opacityImage.width(); ++x) {
            auto* pixel = row + x * 4;
            const uchar value = x < 4 ? 0 : 255;
            pixel[0] = pixel[1] = pixel[2] = 255 - value;
            pixel[3] = value;
        }
    }
    ASSERT_TRUE(opacityImage.save(
        QString::fromStdString(texturePath.string()), "PNG"));

    ArtifactCore::RTPbrMaterial backMaterial;
    backMaterial.baseColor[0] = backMaterial.baseColor[1] =
        backMaterial.baseColor[2] = 0.0f;
    backMaterial.metallic = 1.0f;
    backMaterial.emissionColor[2] = 1.0f;
    backMaterial.emissionStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), backMaterial, 0));

    ArtifactCore::RTPbrMaterial frontMaterial;
    frontMaterial.baseColor[0] = frontMaterial.baseColor[1] =
        frontMaterial.baseColor[2] = 0.0f;
    frontMaterial.metallic = 1.0f;
    frontMaterial.emissionColor[0] = 1.0f;
    frontMaterial.emissionStrength = 1.0f;
    frontMaterial.alphaMode = 1;
    frontMaterial.alphaCutoff = 0.5f;
    frontMaterial.opacityTexture = ArtifactCore::UniString(
        QString::fromStdString(texturePath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("27-pbr-masked-opacity.png"));

    const auto cutoutPixel = pixelAt(8, 16);
    const auto opaquePixel = pixelAt(24, 16);
    EXPECT_GT(cutoutPixel[2], cutoutPixel[0] + 0.5f);
    EXPECT_GT(opaquePixel[0], opaquePixel[2] + 0.5f);

    frontMaterial.alphaMode = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto opaqueModePixel = pixelAt(8, 16);
    EXPECT_GT(opaqueModePixel[0], opaqueModePixel[2] + 0.5f)
        << "opaque alpha mode should keep the masked front surface";

    frontMaterial.alphaMode = 2;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto blendedModePixel = pixelAt(8, 16);
    EXPECT_GT(blendedModePixel[0], blendedModePixel[2] + 0.5f)
        << "blended alpha mode should not use binary masked ray cutout";

    frontMaterial.alphaMode = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto restoredCutoutPixel = pixelAt(8, 16);
    EXPECT_GT(restoredCutoutPixel[2], restoredCutoutPixel[0] + 0.5f)
        << "restoring mask mode should rebuild BLAS alpha-test flags";

    frontMaterial.opacityTextureUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedCutoutPixel = pixelAt(8, 16);
    const auto rotatedOpaquePixel = pixelAt(24, 16);
    EXPECT_GT(rotatedCutoutPixel[0], rotatedCutoutPixel[2] + 0.5f)
        << "rotating the opacity map should move its opaque half onto the left side";
    EXPECT_GT(rotatedOpaquePixel[2], rotatedOpaquePixel[0] + 0.5f)
        << "rotating the opacity map should move its cutout half onto the right side";

    frontMaterial.opacityTextureUvRotationDegrees = 0.0f;
    frontMaterial.opacityTextureUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1OpaquePixel = pixelAt(8, 16);
    const auto uv1CutoutPixel = pixelAt(24, 16);
    EXPECT_GT(uv1OpaquePixel[0], uv1OpaquePixel[2] + 0.5f)
        << "opacity UV1 mirroring should move the opaque texels to the left";
    EXPECT_GT(uv1CutoutPixel[2], uv1CutoutPixel[0] + 0.5f)
        << "opacity UV1 mirroring should move the cutout texels to the right";

    frontMaterial.opacityTextureUvSet = 0;
    frontMaterial.baseColorAlpha = 0.7f;
    frontMaterial.opacity = 0.7f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("28-pbr-masked-alpha-factors.png"));
    const auto factorCutoutPixel = pixelAt(24, 16);
    EXPECT_GT(factorCutoutPixel[2], factorCutoutPixel[0] + 0.5f)
        << "base color alpha * scalar opacity must be below alpha cutoff";

    frontMaterial.baseColorAlpha = 0.8f;
    frontMaterial.opacity = 0.625f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto cutoffBoundaryPixel = pixelAt(24, 16);
    EXPECT_GT(cutoffBoundaryPixel[0], cutoffBoundaryPixel[2] + 0.5f)
        << "alpha equal to the cutoff should keep the front surface";

    frontMaterial.opacity = 0.6375f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto aboveCutoffPixel = pixelAt(24, 16);
    EXPECT_GT(aboveCutoffPixel[0], aboveCutoffPixel[2] + 0.5f)
        << "alpha just above the cutoff should keep the front surface";

    frontMaterial.opacityTexture = ArtifactCore::UniString{};
    frontMaterial.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(texturePath.string()));
    frontMaterial.baseColorAlpha = 1.0f;
    frontMaterial.opacity = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-alpha-surfaces"), frontMaterial, 1));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto baseColorTextureCutoutPixel = pixelAt(8, 16);
    const auto baseColorTextureOpaquePixel = pixelAt(24, 16);
    EXPECT_GT(baseColorTextureCutoutPixel[2],
              baseColorTextureCutoutPixel[0] + 0.5f)
        << "zero base-color texture alpha should continue the ray to the rear";
    EXPECT_GT(baseColorTextureOpaquePixel[0],
              baseColorTextureOpaquePixel[2] + 0.5f)
        << "full base-color texture alpha should keep the masked front surface";
}

TEST_F(RayTracingAcceptanceTest, MaskedOpacityTexturePassesShadowRay) {
    constexpr std::array<float, 12> receiverVertices{
        -1.2f, -1.0f, 0.4f, 1.2f, -1.0f, 0.4f,
         1.2f,  1.0f, 0.4f, -1.2f, 1.0f, 0.4f};
    constexpr std::array<std::uint32_t, 6> receiverIndices{
        0, 2, 1, 0, 3, 2};
    constexpr std::array<float, 12> blockerVertices{
         0.16f, -0.35f, 0.0f, 0.27f, -0.35f, 0.0f,
         0.27f,  0.35f, 0.0f, 0.16f,  0.35f, 0.0f};
    constexpr std::array<std::uint32_t, 6> blockerIndices{
        0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> blockerUvs{
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 16> opaqueBlockerVertexColors{
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    constexpr std::array<float, 16> transparentBlockerVertexColors{
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f};
    constexpr std::array<float, 16> partialBlockerVertexColors{
        1.0f, 1.0f, 1.0f, 0.6f, 1.0f, 1.0f, 1.0f, 0.6f,
        1.0f, 1.0f, 1.0f, 0.6f, 1.0f, 1.0f, 1.0f, 0.6f};

    auto receiverVertexBuffer = makeBuffer(
        receiverVertices.data(), sizeof(receiverVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto receiverIndexBuffer = makeBuffer(
        receiverIndices.data(), sizeof(receiverIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto blockerVertexBuffer = makeBuffer(
        blockerVertices.data(), sizeof(blockerVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto blockerIndexBuffer = makeBuffer(
        blockerIndices.data(), sizeof(blockerIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(receiverVertexBuffer, nullptr);
    ASSERT_NE(receiverIndexBuffer, nullptr);
    ASSERT_NE(blockerVertexBuffer, nullptr);
    ASSERT_NE(blockerIndexBuffer, nullptr);

    addGeometry("masked-shadow-receiver", receiverVertexBuffer.RawPtr(), 4,
                receiverIndexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                receiverVertices.data(), receiverIndices.data());
    addGeometry("masked-shadow-blocker", blockerVertexBuffer.RawPtr(), 4,
                blockerIndexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                blockerVertices.data(), blockerIndices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerVertices.data(),
        4, blockerIndices.data(), 6, nullptr, nullptr, blockerUvs.data(),
        opaqueBlockerVertexColors.data()));

    ArtifactCore::RTPbrMaterial receiverMaterial;
    receiverMaterial.baseColor[0] = receiverMaterial.baseColor[1] =
        receiverMaterial.baseColor[2] = 0.8f;
    receiverMaterial.roughness = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-receiver"), receiverMaterial));

    QTemporaryDir shadowOpacityDirectory;
    ASSERT_TRUE(shadowOpacityDirectory.isValid());
    const auto whiteMapPath = std::filesystem::path(
        shadowOpacityDirectory.filePath(QStringLiteral("opaque.png"))
            .toStdString());
    const auto transparentMapPath = std::filesystem::path(
        shadowOpacityDirectory.filePath(QStringLiteral("transparent.png"))
            .toStdString());
    const auto cutoffEqualMapPath = std::filesystem::path(
        shadowOpacityDirectory.filePath(QStringLiteral("cutoff-equal.png"))
            .toStdString());
    const auto belowCutoffMapPath = std::filesystem::path(
        shadowOpacityDirectory.filePath(QStringLiteral("below-cutoff.png"))
            .toStdString());
    const auto transparentBaseColorMapPath = std::filesystem::path(
        shadowOpacityDirectory.filePath(
            QStringLiteral("base-color-transparent-alpha.png"))
            .toStdString());
    const auto opaqueBaseColorMapPath = std::filesystem::path(
        shadowOpacityDirectory.filePath(
            QStringLiteral("base-color-opaque-alpha.png"))
            .toStdString());
    for (const auto& item : std::array<std::pair<std::filesystem::path, QColor>, 4>{
             std::pair{whiteMapPath, QColor(255, 255, 255, 255)},
             std::pair{transparentMapPath, QColor(0, 0, 0, 0)},
             std::pair{cutoffEqualMapPath, QColor(255, 255, 255, 128)},
             std::pair{belowCutoffMapPath, QColor(255, 255, 255, 127)}}) {
        QImage image(2, 2, QImage::Format_RGBA8888);
        ASSERT_FALSE(image.isNull());
        image.fill(item.second);
        ASSERT_TRUE(image.save(QString::fromStdString(item.first.string()), "PNG"));
    }
    for (const auto& item :
         std::array<std::pair<std::filesystem::path, QColor>, 2>{
             std::pair{transparentBaseColorMapPath, QColor(255, 255, 255, 0)},
             std::pair{opaqueBaseColorMapPath, QColor(0, 0, 0, 255)}}) {
        QImage image(2, 2, QImage::Format_RGBA8888);
        ASSERT_FALSE(image.isNull());
        image.fill(item.second);
        ASSERT_TRUE(image.save(QString::fromStdString(item.first.string()), "PNG"));
    }

    ArtifactCore::RTPbrMaterial blockerMaterial;
    blockerMaterial.alphaMode = 1;
    blockerMaterial.alphaCutoff = 0.5f;
    blockerMaterial.baseColor[0] = blockerMaterial.baseColor[1] =
        blockerMaterial.baseColor[2] = 0.0f;
    blockerMaterial.opacityTexture = ArtifactCore::UniString(
        QString::fromStdString(whiteMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));

    ArtifactCore::RTSceneLight pointLight;
    pointLight.positionType[0] = -1.0f;
    pointLight.positionType[2] = -1.0f;
    pointLight.positionType[3] = static_cast<float>(ArtifactCore::LightType::Point);
    pointLight.directionIntensity[3] = 24.0f;
    pointLight.colorAttenuationConstant[1] = 1.0f;
    pointLight.colorAttenuationConstant[3] = 1.0f;
    pointLight.shadowParams[0] = 0.0f;
    manager_->setSceneLights(context_.RawPtr(), &pointLight, 1);
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto opaqueBlockerPixel = pixelAt(24, 16);

    blockerMaterial.opacityTexture = ArtifactCore::UniString(
        QString::fromStdString(transparentMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("30-pbr-masked-shadow-ray.png"));
    const auto transparentBlockerPixel = pixelAt(24, 16);
    EXPECT_GT(transparentBlockerPixel[1], opaqueBlockerPixel[1] + 0.02f)
        << "a cutout blocker should not terminate the receiver's shadow ray";

    blockerMaterial.alphaMode = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto opaqueModeBlockerPixel = pixelAt(24, 16);
    EXPECT_NEAR(opaqueModeBlockerPixel[1], opaqueBlockerPixel[1], 0.02f)
        << "opaque alpha mode should ignore transparent opacity texture in shadows";

    blockerMaterial.alphaMode = 2;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto blendedModeBlockerPixel = pixelAt(24, 16);
    EXPECT_NEAR(blendedModeBlockerPixel[1], opaqueBlockerPixel[1], 0.02f)
        << "blended alpha mode should not use binary masked shadow cutout";

    blockerMaterial.alphaMode = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));

    blockerMaterial.alphaCutoff = 128.0f / 255.0f;
    blockerMaterial.opacityTexture = ArtifactCore::UniString(
        QString::fromStdString(cutoffEqualMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto cutoffEqualPixel = pixelAt(24, 16);
    EXPECT_NEAR(cutoffEqualPixel[1], opaqueBlockerPixel[1], 0.02f)
        << "alpha equal to the cutoff should block the shadow ray";

    blockerMaterial.opacityTexture = ArtifactCore::UniString(
        QString::fromStdString(belowCutoffMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto belowCutoffPixel = pixelAt(24, 16);
    EXPECT_NEAR(belowCutoffPixel[1], transparentBlockerPixel[1], 0.02f)
        << "alpha below the cutoff should pass the shadow ray";

    QTemporaryDir missingOpacityDirectory;
    ASSERT_TRUE(missingOpacityDirectory.isValid());
    blockerMaterial.opacityTexture = ArtifactCore::UniString(
        missingOpacityDirectory.filePath(QStringLiteral("missing-opacity.png")));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto missingOpacityPixel = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(missingOpacityPixel[channel], opaqueBlockerPixel[channel],
                    0.02f)
            << "a missing opacity map should fall back to scalar opacity, "
               "channel " << channel;
    }

    blockerMaterial.opacityTexture = ArtifactCore::UniString();
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerVertices.data(),
        4, blockerIndices.data(), 6, nullptr, nullptr, blockerUvs.data(),
        transparentBlockerVertexColors.data()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto transparentVertexAlphaPixel = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(transparentVertexAlphaPixel[channel],
                    transparentBlockerPixel[channel], 0.02f)
            << "zero vertex alpha should pass the masked shadow ray, channel "
            << channel;
    }

    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerVertices.data(),
        4, blockerIndices.data(), 6, nullptr, nullptr, blockerUvs.data(),
        opaqueBlockerVertexColors.data()));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto opaqueVertexAlphaPixel = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(opaqueVertexAlphaPixel[channel], opaqueBlockerPixel[channel],
                    0.02f)
            << "full vertex alpha should block the masked shadow ray, channel "
            << channel;
    }

    blockerMaterial.baseColorAlpha = 0.9f;
    blockerMaterial.opacity = 0.9f;
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerVertices.data(),
        4, blockerIndices.data(), 6, nullptr, nullptr, blockerUvs.data(),
        partialBlockerVertexColors.data()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto combinedVertexMaterialAlphaOpen = pixelAt(24, 16);
    EXPECT_NEAR(combinedVertexMaterialAlphaOpen[1],
                transparentBlockerPixel[1], 0.02f)
        << "0.9 base alpha * 0.9 opacity * 0.6 vertex alpha should pass below cutoff";

    blockerMaterial.opacity = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto combinedVertexMaterialAlphaBlocked = pixelAt(24, 16);
    EXPECT_NEAR(combinedVertexMaterialAlphaBlocked[1],
                opaqueBlockerPixel[1], 0.02f)
        << "0.9 base alpha * full opacity * 0.6 vertex alpha should block above cutoff";

    blockerMaterial.baseColorAlpha = 1.0f;
    blockerMaterial.opacity = 1.0f;
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerVertices.data(),
        4, blockerIndices.data(), 6, nullptr, nullptr, blockerUvs.data(),
        opaqueBlockerVertexColors.data()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));

    blockerMaterial.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(transparentBaseColorMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto transparentBaseColorAlphaPixel = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(transparentBaseColorAlphaPixel[channel],
                    transparentBlockerPixel[channel], 0.02f)
            << "zero base-color texture alpha should pass the masked shadow ray, "
               "channel " << channel;
    }

    blockerMaterial.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(opaqueBaseColorMapPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto opaqueBaseColorAlphaPixel = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(opaqueBaseColorAlphaPixel[channel], opaqueBlockerPixel[channel],
                    0.02f)
            << "full base-color texture alpha should block the masked shadow ray, "
               "channel " << channel;
    }

    blockerMaterial.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(cutoffEqualMapPath.string()));
    blockerMaterial.opacity = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto combinedAlphaAtCutoffPixel = pixelAt(24, 16);
    EXPECT_NEAR(combinedAlphaAtCutoffPixel[1], opaqueBlockerPixel[1], 0.02f)
        << "half-alpha base-color texture times full opacity should block";

    blockerMaterial.opacity = 0.99f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-blocker"), blockerMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto combinedAlphaBelowCutoffPixel = pixelAt(24, 16);
    EXPECT_NEAR(combinedAlphaBelowCutoffPixel[1], transparentBlockerPixel[1],
                0.02f)
        << "half-alpha base-color texture times scalar opacity should pass below cutoff";

}

TEST_F(RayTracingAcceptanceTest,
       SelectsMaskedShadowOpacityTexturePerMaterialSlot) {
    constexpr std::array<float, 12> receiverVertices{
        -1.2f, -1.0f, 0.4f, 1.2f, -1.0f, 0.4f,
         1.2f,  1.0f, 0.4f, -1.2f, 1.0f, 0.4f};
    constexpr std::array<std::uint32_t, 6> receiverIndices{
        0, 2, 1, 0, 3, 2};
    constexpr std::array<float, 24> blockerVertices{
        -0.35f, -0.3f, 0.0f, -0.22f, -0.3f, 0.0f,
        -0.22f,  0.3f, 0.0f, -0.35f,  0.3f, 0.0f,
         0.22f, -0.3f, 0.0f,  0.35f, -0.3f, 0.0f,
         0.35f,  0.3f, 0.0f,  0.22f,  0.3f, 0.0f};
    constexpr std::array<std::uint32_t, 12> blockerIndices{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    constexpr std::array<std::uint32_t, 4> blockerMaterialIndices{0, 0, 1, 1};
    constexpr std::array<float, 16> blockerUvs{
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    auto receiverVertexBuffer = makeBuffer(
        receiverVertices.data(), sizeof(receiverVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto receiverIndexBuffer = makeBuffer(
        receiverIndices.data(), sizeof(receiverIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto blockerVertexBuffer = makeBuffer(
        blockerVertices.data(), sizeof(blockerVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto blockerIndexBuffer = makeBuffer(
        blockerIndices.data(), sizeof(blockerIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(receiverVertexBuffer, nullptr);
    ASSERT_NE(receiverIndexBuffer, nullptr);
    ASSERT_NE(blockerVertexBuffer, nullptr);
    ASSERT_NE(blockerIndexBuffer, nullptr);
    addGeometry("multi-slot-shadow-receiver", receiverVertexBuffer.RawPtr(), 4,
                receiverIndexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                receiverVertices.data(), receiverIndices.data());
    addGeometry("multi-slot-shadow-blockers", blockerVertexBuffer.RawPtr(), 8,
                blockerIndexBuffer.RawPtr(), 12, float4x4::Identity(), 1, 1,
                blockerVertices.data(), blockerIndices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        blockerVertices.data(), 8, blockerIndices.data(), 12, nullptr,
        blockerMaterialIndices.data(), blockerUvs.data()));

    QTemporaryDir opacityDirectory;
    ASSERT_TRUE(opacityDirectory.isValid());
    const auto transparentPath = opacityDirectory.filePath(
        QStringLiteral("transparent-shadow.png"));
    const auto opaquePath = opacityDirectory.filePath(
        QStringLiteral("opaque-shadow.png"));
    QImage transparentImage(2, 2, QImage::Format_RGBA8888);
    QImage opaqueImage(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(transparentImage.isNull());
    ASSERT_FALSE(opaqueImage.isNull());
    transparentImage.fill(QColor(255, 255, 255, 0));
    opaqueImage.fill(QColor(255, 255, 255, 255));
    ASSERT_TRUE(transparentImage.save(transparentPath, "PNG"));
    ASSERT_TRUE(opaqueImage.save(opaquePath, "PNG"));

    ArtifactCore::RTPbrMaterial receiverMaterial;
    receiverMaterial.baseColor[0] = receiverMaterial.baseColor[1] =
        receiverMaterial.baseColor[2] = 0.8f;
    receiverMaterial.roughness = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-receiver"),
        receiverMaterial));
    ArtifactCore::RTPbrMaterial slot0Material;
    slot0Material.alphaMode = 1;
    slot0Material.alphaCutoff = 0.5f;
    slot0Material.opacityTexture = ArtifactCore::UniString(opaquePath);
    ArtifactCore::RTPbrMaterial slot1Material = slot0Material;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));

    const auto traceReceiverGreen = [&](const float lightX) {
        ArtifactCore::RTSceneLight pointLight;
        pointLight.positionType[0] = lightX;
        pointLight.positionType[2] = -1.0f;
        pointLight.positionType[3] =
            static_cast<float>(ArtifactCore::LightType::Point);
        pointLight.directionIntensity[3] = 24.0f;
        pointLight.colorAttenuationConstant[1] = 1.0f;
        pointLight.colorAttenuationConstant[3] = 1.0f;
        pointLight.shadowParams[0] = 0.0f;
        manager_->setSceneLights(context_.RawPtr(), &pointLight, 1);
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(16, 16)[1];
    };

    slot0Material.opacityTexture = ArtifactCore::UniString(transparentPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    const float leftSlotOpen = traceReceiverGreen(-1.0f);
    slot0Material.opacityTexture = ArtifactCore::UniString(opaquePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    const float leftSlotBlocked = traceReceiverGreen(-1.0f);
    EXPECT_GT(leftSlotOpen, leftSlotBlocked + 0.02f)
        << "slot 0 opacity should control the left blocker shadow";

    slot1Material.opacityTexture = ArtifactCore::UniString(transparentPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float rightSlotOpen = traceReceiverGreen(1.0f);
    slot1Material.opacityTexture = ArtifactCore::UniString(opaquePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float rightSlotBlocked = traceReceiverGreen(1.0f);
    EXPECT_GT(rightSlotOpen, rightSlotBlocked + 0.02f)
        << "slot 1 opacity should control the right blocker shadow";

    slot0Material.opacityTexture = ArtifactCore::UniString();
    slot1Material.opacityTexture = ArtifactCore::UniString();
    slot0Material.opacity = 0.0f;
    slot1Material.opacity = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftScalarOpen = traceReceiverGreen(-1.0f);
    const float rightScalarBlocked = traceReceiverGreen(1.0f);
    EXPECT_GT(leftScalarOpen, leftSlotBlocked + 0.02f)
        << "zero scalar opacity on slot 0 should pass its shadow ray";
    EXPECT_NEAR(rightScalarBlocked, rightSlotBlocked, 0.02f)
        << "full scalar opacity on slot 1 should preserve its blocker shadow";

    slot0Material.opacity = 1.0f;
    slot1Material.opacity = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftScalarBlocked = traceReceiverGreen(-1.0f);
    const float rightScalarOpen = traceReceiverGreen(1.0f);
    EXPECT_NEAR(leftScalarBlocked, leftSlotBlocked, 0.02f)
        << "full scalar opacity on slot 0 should preserve its blocker shadow";
    EXPECT_GT(rightScalarOpen, rightSlotBlocked + 0.02f)
        << "zero scalar opacity on slot 1 should pass its shadow ray";

    slot0Material.opacity = 0.7f;
    slot0Material.baseColorAlpha = 0.7f;
    slot1Material.opacity = 0.8f;
    slot1Material.baseColorAlpha = 0.8f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftCombinedAlphaOpen = traceReceiverGreen(-1.0f);
    const float rightCombinedAlphaBlocked = traceReceiverGreen(1.0f);
    EXPECT_GT(leftCombinedAlphaOpen, leftSlotBlocked + 0.02f)
        << "slot 0 base-color alpha times scalar opacity is below cutoff";
    EXPECT_NEAR(rightCombinedAlphaBlocked, rightSlotBlocked, 0.02f)
        << "slot 1 base-color alpha times scalar opacity is above cutoff";

    slot0Material.opacity = 0.8f;
    slot0Material.baseColorAlpha = 0.8f;
    slot1Material.opacity = 0.7f;
    slot1Material.baseColorAlpha = 0.7f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftCombinedAlphaBlocked = traceReceiverGreen(-1.0f);
    const float rightCombinedAlphaOpen = traceReceiverGreen(1.0f);
    EXPECT_NEAR(leftCombinedAlphaBlocked, leftSlotBlocked, 0.02f)
        << "slot 0 base-color alpha times scalar opacity is above cutoff";
    EXPECT_GT(rightCombinedAlphaOpen, rightSlotBlocked + 0.02f)
        << "slot 1 base-color alpha times scalar opacity is below cutoff";

    slot0Material.alphaMode = 0;
    slot1Material.alphaMode = 1;
    slot0Material.opacityTexture = ArtifactCore::UniString(transparentPath);
    slot1Material.opacityTexture = ArtifactCore::UniString(transparentPath);
    slot0Material.opacity = slot1Material.opacity = 1.0f;
    slot0Material.baseColorAlpha = slot1Material.baseColorAlpha = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftOpaqueModeBlocked = traceReceiverGreen(-1.0f);
    const float rightMaskedModeOpen = traceReceiverGreen(1.0f);
    EXPECT_NEAR(leftOpaqueModeBlocked, leftSlotBlocked, 0.02f)
        << "opaque mode on slot 0 should ignore transparent shadow alpha";
    EXPECT_GT(rightMaskedModeOpen, rightSlotBlocked + 0.02f)
        << "masked mode on slot 1 should pass its transparent shadow";

    slot0Material.alphaMode = 1;
    slot1Material.alphaMode = 0;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftMaskedModeOpen = traceReceiverGreen(-1.0f);
    const float rightOpaqueModeBlocked = traceReceiverGreen(1.0f);
    EXPECT_GT(leftMaskedModeOpen, leftSlotBlocked + 0.02f)
        << "masked mode on slot 0 should pass its transparent shadow";
    EXPECT_NEAR(rightOpaqueModeBlocked, rightSlotBlocked, 0.02f)
        << "opaque mode on slot 1 should ignore transparent shadow alpha";

    slot0Material.alphaMode = slot1Material.alphaMode = 1;
    slot0Material.opacityTexture = ArtifactCore::UniString();
    slot1Material.opacityTexture = ArtifactCore::UniString();
    slot0Material.baseColorAlpha = slot1Material.baseColorAlpha = 1.0f;
    slot0Material.opacity = slot1Material.opacity = 0.45f;
    slot0Material.alphaCutoff = 0.4f;
    slot1Material.alphaCutoff = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftLowCutoffBlocked = traceReceiverGreen(-1.0f);
    const float rightHighCutoffOpen = traceReceiverGreen(1.0f);
    EXPECT_NEAR(leftLowCutoffBlocked, leftSlotBlocked, 0.02f)
        << "slot 0's lower alpha cutoff should retain 0.45 opacity";
    EXPECT_GT(rightHighCutoffOpen, rightSlotBlocked + 0.02f)
        << "slot 1's higher alpha cutoff should pass 0.45 opacity";

    slot0Material.alphaCutoff = 0.5f;
    slot1Material.alphaCutoff = 0.4f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftHighCutoffOpen = traceReceiverGreen(-1.0f);
    const float rightLowCutoffBlocked = traceReceiverGreen(1.0f);
    EXPECT_GT(leftHighCutoffOpen, leftSlotBlocked + 0.02f)
        << "slot 0's higher alpha cutoff should pass 0.45 opacity";
    EXPECT_NEAR(rightLowCutoffBlocked, rightSlotBlocked, 0.02f)
        << "slot 1's lower alpha cutoff should retain 0.45 opacity";

    slot0Material.alphaCutoff = 0.45f;
    slot1Material.alphaCutoff = 0.4501f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftEqualCutoffBlocked = traceReceiverGreen(-1.0f);
    const float rightAboveCutoffOpen = traceReceiverGreen(1.0f);
    EXPECT_NEAR(leftEqualCutoffBlocked, leftSlotBlocked, 0.02f)
        << "alpha equal to slot 0 cutoff should retain the shadow blocker";
    EXPECT_GT(rightAboveCutoffOpen, rightSlotBlocked + 0.02f)
        << "slot 1 cutoff just above alpha should pass the shadow ray";

    slot0Material.alphaCutoff = 0.4501f;
    slot1Material.alphaCutoff = 0.45f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot0Material, 0));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("multi-slot-shadow-blockers"),
        slot1Material, 1));
    const float leftAboveCutoffOpen = traceReceiverGreen(-1.0f);
    const float rightEqualCutoffBlocked = traceReceiverGreen(1.0f);
    EXPECT_GT(leftAboveCutoffOpen, leftSlotBlocked + 0.02f)
        << "slot 0 cutoff just above alpha should pass the shadow ray";
    EXPECT_NEAR(rightEqualCutoffBlocked, rightSlotBlocked, 0.02f)
        << "alpha equal to slot 1 cutoff should retain the shadow blocker";
}

TEST_F(RayTracingAcceptanceTest,
       SelectsLinearMipLevelsForMinifiedMetallicRoughnessTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-metallic-roughness-mip", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-metallic-roughness-mip"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("metallic-roughness-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("metallic-roughness-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage average(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = averagePixel[0] = 255;
            checkerPixel[1] = averagePixel[1] = 128;
            checkerPixel[2] = ((x + y) & 1) != 0 ? 255 : 0;
            averagePixel[2] = 128;
            checkerPixel[3] = averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.metallic = 1.0f;
    material.roughness = 0.8f;
    material.metallicRoughnessTexture = ArtifactCore::UniString(checkerPath);
    const auto traceAndSample = [&](const ArtifactCore::RTPbrMaterial& input,
                                    const int x) {
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-metallic-roughness-mip"), input));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(x, 16)[0];
    };
    std::array<float, 5> checkerSamples{};
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }

    material.metallicRoughnessTexture = ArtifactCore::UniString(averagePath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        const float reference = traceAndSample(material, 14 + index);
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)], reference,
                    0.035f)
            << "minified packed metallic-roughness sample " << index
            << " should match the linear mip average";
    }
}

TEST_F(RayTracingAcceptanceTest,
       SelectsLinearMipLevelsForMinifiedIndependentRoughnessTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-roughness-mip", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-roughness-mip"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("roughness-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("roughness-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage average(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = ((x + y) & 1) != 0 ? 255 : 0;
            averagePixel[0] = 128;
            checkerPixel[1] = averagePixel[1] = 255;
            checkerPixel[2] = averagePixel[2] = 0;
            checkerPixel[3] = averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.roughness = 1.0f;
    material.roughnessTexture = ArtifactCore::UniString(checkerPath);
    const auto traceAndSample = [&](const ArtifactCore::RTPbrMaterial& input,
                                    const int x) {
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-roughness-mip"), input));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(x, 16)[0];
    };
    std::array<float, 5> checkerSamples{};
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }

    material.roughnessTexture = ArtifactCore::UniString(averagePath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        const float reference = traceAndSample(material, 14 + index);
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)], reference,
                    0.035f)
            << "minified independent roughness sample " << index
            << " should match its linear mip average";
    }
}

TEST_F(RayTracingAcceptanceTest,
       SelectsLinearMipLevelsForMinifiedIndependentMetallicTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-metallic-mip", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-metallic-mip"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("metallic-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("metallic-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage average(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = ((x + y) & 1) != 0 ? 255 : 0;
            averagePixel[0] = 128;
            checkerPixel[1] = averagePixel[1] = 255;
            checkerPixel[2] = averagePixel[2] = 255;
            checkerPixel[3] = averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.metallic = 1.0f;
    material.roughness = 0.5f;
    material.metallicTexture = ArtifactCore::UniString(checkerPath);
    const auto traceAndSample = [&](const ArtifactCore::RTPbrMaterial& input,
                                    const int x) {
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-metallic-mip"), input));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(x, 16)[0];
    };
    std::array<float, 5> checkerSamples{};
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }

    material.metallicTexture = ArtifactCore::UniString(averagePath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        const float reference = traceAndSample(material, 14 + index);
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)], reference,
                    0.035f)
            << "minified independent metallic sample " << index
            << " should match its linear mip average";
    }
}

TEST_F(RayTracingAcceptanceTest,
       SelectsMipLevelsForMinifiedNonPowerOfTwoPbrTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-mip-selection", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-mip-selection"), vertices.data(),
        4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureWidth = 127;
    constexpr int textureHeight = 95;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("pbr-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("pbr-average.png"));
    QImage checker(textureWidth, textureHeight, QImage::Format_RGBA8888);
    QImage average(textureWidth, textureHeight, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureHeight; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureWidth; ++x) {
            const uchar checkerValue = ((x + y) & 1) != 0 ? 255 : 0;
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = checkerValue;
            checkerPixel[1] = checkerValue;
            checkerPixel[2] = checkerValue;
            checkerPixel[3] = 255;
            averagePixel[0] = 188;
            averagePixel[1] = 188;
            averagePixel[2] = 188;
            averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 1.0f;
    material.baseColorTexture = ArtifactCore::UniString(checkerPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-minified-mip-selection"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("25-pbr-minified-texture-mips.png"));
    std::array<float, 5> checkerSamples{};
    for (Uint32 i = 0; i < checkerSamples.size(); ++i) {
        checkerSamples[i] = pixelAt(14 + i, 16)[0];
    }

    material.baseColorTexture = ArtifactCore::UniString(averagePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-minified-mip-selection"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    for (Uint32 i = 0; i < checkerSamples.size(); ++i) {
        const float averageSample = pixelAt(14 + i, 16)[0];
        EXPECT_NEAR(checkerSamples[i], averageSample, 0.025f)
            << "minified checker sample " << i
            << " should match its linear-light mip average";
    }
}

TEST_F(RayTracingAcceptanceTest,
       SelectsLinearLightMipLevelsForMinifiedEmissionTexturesAtMultipleUvScales) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> highDensityUvs1{
        0.0f, 0.0f, 4.0f, 0.0f, 4.0f, 4.0f, 0.0f, 4.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-emission-mip-selection", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-emission-mip-selection"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data(),
        nullptr, highDensityUvs1.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("emission-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("emission-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage average(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            const uchar checkerValue = ((x + y) & 1) != 0 ? 255 : 0;
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = checkerValue;
            checkerPixel[1] = checkerValue;
            checkerPixel[2] = checkerValue;
            checkerPixel[3] = 255;
            averagePixel[0] = 188;
            averagePixel[1] = 188;
            averagePixel[2] = 188;
            averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.0f;
    material.specular = 0.0f;
    material.ior = 1.0f;
    material.emissionColor[0] = material.emissionColor[1] =
        material.emissionColor[2] = 1.0f;
    material.emissionStrength = 1.0f;
    for (const float uvScale : {0.5f, 1.0f, 4.0f}) {
        material.emissionTexture = ArtifactCore::UniString(checkerPath);
        material.emissionTextureUvScale = uvScale;
        ASSERT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-emission-mip-selection"), material));
        ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        const std::array<float, 5> checkerSamples{
            pixelAt(14, 16)[0], pixelAt(15, 16)[0], pixelAt(16, 16)[0],
            pixelAt(17, 16)[0], pixelAt(18, 16)[0]};

        material.emissionTexture = ArtifactCore::UniString(averagePath);
        ASSERT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-emission-mip-selection"), material));
        ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        for (Uint32 index = 0; index < checkerSamples.size(); ++index) {
            EXPECT_NEAR(checkerSamples[index], pixelAt(14 + index, 16)[0], 0.025f)
                << "minified emission checker sample " << index
                << " should match its linear-light mip average at UV scale "
                << uvScale;
        }
    }

    material.emissionTextureUvSet = 1;
    material.emissionTextureUvScale = 1.0f;
    material.emissionTexture = ArtifactCore::UniString(checkerPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-minified-emission-mip-selection"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const std::array<float, 5> uv1CheckerSamples{
        pixelAt(14, 16)[0], pixelAt(15, 16)[0], pixelAt(16, 16)[0],
        pixelAt(17, 16)[0], pixelAt(18, 16)[0]};
    material.emissionTexture = ArtifactCore::UniString(averagePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-minified-emission-mip-selection"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    for (int index = 0; index < static_cast<int>(uv1CheckerSamples.size()); ++index) {
        EXPECT_NEAR(uv1CheckerSamples[static_cast<std::size_t>(index)],
                    pixelAt(14 + index, 16)[0], 0.035f)
            << "high-density UV1 emission sample " << index
            << " should select the linear-light mip average";
    }
}

TEST_F(RayTracingAcceptanceTest,
       SelectsLinearMipLevelsForMinifiedClearcoatAndRoughnessTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-clearcoat-roughness-mip", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-clearcoat-roughness-mip"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("clearcoat-checker.png"));
    const auto averagePath =
        tempDirectory.filePath(QStringLiteral("clearcoat-average.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage average(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = ((x + y) & 1) != 0 ? 255 : 0;
            checkerPixel[1] = ((x + y) & 1) != 0 ? 255 : 0;
            checkerPixel[2] = 0;
            checkerPixel[3] = 255;
            averagePixel[0] = 127;
            averagePixel[1] = 128;
            averagePixel[2] = 0;
            averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(average.save(averagePath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.roughness = 0.8f;
    material.clearcoat = 1.0f;
    material.clearcoatRoughness = 0.5f;
    material.clearcoatTexture = ArtifactCore::UniString(checkerPath);
    material.clearcoatRoughnessTexture = ArtifactCore::UniString(checkerPath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-minified-clearcoat-roughness-mip"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const std::array<float, 5> checkerSamples{
        pixelAt(14, 16)[0], pixelAt(15, 16)[0], pixelAt(16, 16)[0],
        pixelAt(17, 16)[0], pixelAt(18, 16)[0]};

    material.clearcoatTexture = ArtifactCore::UniString(averagePath);
    material.clearcoatRoughnessTexture = ArtifactCore::UniString(averagePath);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-minified-clearcoat-roughness-mip"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    for (Uint32 index = 0; index < checkerSamples.size(); ++index) {
        EXPECT_NEAR(checkerSamples[index], pixelAt(14 + index, 16)[0], 0.025f)
            << "minified clearcoat-roughness sample " << index
            << " should match its linear mip average";
    }
}

TEST_F(RayTracingAcceptanceTest,
       RenormalizesMinifiedNormalMipsAndAppliesNormalVarianceRoughness) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-minified-normal-mip-selection", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-minified-normal-mip-selection"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir tempDirectory;
    ASSERT_TRUE(tempDirectory.isValid());
    const auto checkerPath =
        tempDirectory.filePath(QStringLiteral("normal-checker.png"));
    const auto averageDirectionPath =
        tempDirectory.filePath(QStringLiteral("normal-average-direction.png"));
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage averageDirection(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(averageDirection.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = averageDirection.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            // Alternating tangent normals are (+/-0.6, 0, 0.8). Their
            // normalized mip direction is flat, with pre-normalization length 0.8.
            checkerPixel[0] = ((x + y) & 1) != 0 ? 204 : 51;
            checkerPixel[1] = 128;
            checkerPixel[2] = 230;
            checkerPixel[3] = 255;
            averagePixel[0] = 128;
            averagePixel[1] = 128;
            averagePixel[2] = 255;
            averagePixel[3] = 255;
        }
    }
    ASSERT_TRUE(checker.save(checkerPath, "PNG"));
    ASSERT_TRUE(averageDirection.save(averageDirectionPath, "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.roughness = 0.5f;
    material.normalStrength = 1.0f;
    material.normalTexture = ArtifactCore::UniString(checkerPath);
    const auto traceAndSample = [&](const ArtifactCore::RTPbrMaterial& input,
                                    const int x) {
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-minified-normal-mip-selection"), input));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return pixelAt(x, 16)[0];
    };
    std::array<float, 5> checkerSamples{};
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        checkerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }

    // The mip chain stores the average normal length (205/255). The RT shader
    // converts that variance into the equivalent roughness before shading.
    constexpr float baseAlpha = 0.5f * 0.5f;
    constexpr float averageNormalLength = 205.0f / 255.0f;
    constexpr float normalVariance =
        (1.0f - averageNormalLength) / averageNormalLength;
    const float filteredRoughness = std::sqrt(std::sqrt(
        baseAlpha * baseAlpha + std::min(2.0f * normalVariance, 0.2f)));
    material.roughness = filteredRoughness;
    material.normalTexture = ArtifactCore::UniString(averageDirectionPath);
    for (int index = 0; index < static_cast<int>(checkerSamples.size()); ++index) {
        const float reference = traceAndSample(material, 14 + index);
        EXPECT_NEAR(checkerSamples[static_cast<std::size_t>(index)], reference,
                    0.035f)
            << "minified normal mip sample " << index
            << " should match its normalized direction and variance-adjusted roughness";
    }

    material.normalTexture = ArtifactCore::UniString();
    material.normalStrength = 0.0f;
    material.clearcoat = 1.0f;
    material.clearcoatRoughness = 0.5f;
    material.clearcoatNormalStrength = 1.0f;
    material.clearcoatNormalTexture = ArtifactCore::UniString(checkerPath);
    std::array<float, 5> coatCheckerSamples{};
    for (int index = 0; index < static_cast<int>(coatCheckerSamples.size()); ++index) {
        coatCheckerSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }
    material.clearcoatRoughness = filteredRoughness;
    material.clearcoatNormalTexture = ArtifactCore::UniString(averageDirectionPath);
    for (int index = 0; index < static_cast<int>(coatCheckerSamples.size()); ++index) {
        EXPECT_NEAR(coatCheckerSamples[static_cast<std::size_t>(index)],
                    traceAndSample(material, 14 + index), 0.04f)
            << "minified clearcoat-normal sample " << index
            << " should match its normalized direction and variance-filtered roughness";
    }

    material.clearcoatNormalTexture = ArtifactCore::UniString();
    std::array<float, 5> coatScalarSamples{};
    for (int index = 0; index < static_cast<int>(coatScalarSamples.size()); ++index) {
        coatScalarSamples[static_cast<std::size_t>(index)] =
            traceAndSample(material, 14 + index);
    }
    const auto missingCoatNormalPath = checkerPath + QStringLiteral(".missing.png");
    ASSERT_FALSE(QFileInfo::exists(missingCoatNormalPath));
    material.clearcoatNormalTexture = ArtifactCore::UniString(missingCoatNormalPath);
    for (int index = 0; index < static_cast<int>(coatScalarSamples.size()); ++index) {
        EXPECT_NEAR(coatScalarSamples[static_cast<std::size_t>(index)],
                    traceAndSample(material, 14 + index), 0.01f)
            << "a missing clearcoat-normal map should preserve scalar coat shading, sample "
            << index;
    }
}

TEST_F(RayTracingAcceptanceTest, SelectsMipLevelsForMinifiedOpacityTextures) {
    constexpr std::array<float, 12> vertices{
        -0.2f, -0.2f, 0.0f, 0.2f, -0.2f, 0.0f,
         0.2f,  0.2f, 0.0f, -0.2f, 0.2f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("opacity-minified-mip-selection", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("opacity-minified-mip-selection"),
        vertices.data(), 4, indices.data(), 6, nullptr, nullptr, uvs.data()));

    constexpr int textureSize = 128;
    QTemporaryDir opacityMipDirectory;
    ASSERT_TRUE(opacityMipDirectory.isValid());
    const auto checkerPath = std::filesystem::path(
        opacityMipDirectory.filePath(QStringLiteral("checker.png")).toStdString());
    const auto averagePath = std::filesystem::path(
        opacityMipDirectory.filePath(QStringLiteral("average.png")).toStdString());
    QImage checker(textureSize, textureSize, QImage::Format_RGBA8888);
    QImage average(textureSize, textureSize, QImage::Format_RGBA8888);
    ASSERT_FALSE(checker.isNull());
    ASSERT_FALSE(average.isNull());
    for (int y = 0; y < textureSize; ++y) {
        auto* checkerRow = checker.scanLine(y);
        auto* averageRow = average.scanLine(y);
        for (int x = 0; x < textureSize; ++x) {
            const uchar alpha = ((x + y) & 1) != 0 ? 255 : 0;
            auto* checkerPixel = checkerRow + x * 4;
            auto* averagePixel = averageRow + x * 4;
            checkerPixel[0] = checkerPixel[1] = checkerPixel[2] = 255;
            checkerPixel[3] = alpha;
            averagePixel[0] = averagePixel[1] = averagePixel[2] = 255;
            averagePixel[3] = 128;
        }
    }
    ASSERT_TRUE(checker.save(QString::fromStdString(checkerPath.string()), "PNG"));
    ASSERT_TRUE(average.save(QString::fromStdString(averagePath.string()), "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 1.0f;
    material.emissionColor[0] = material.emissionColor[1] =
        material.emissionColor[2] = 1.0f;
    material.emissionStrength = 1.0f;
    material.alphaMode = 1;
    material.alphaCutoff = 0.4f;
    material.opacityTexture = ArtifactCore::UniString(
        QString::fromStdString(checkerPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("opacity-minified-mip-selection"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("31-pbr-minified-opacity-mips.png"));
    std::array<float, 5> checkerSamples{};
    for (Uint32 i = 0; i < checkerSamples.size(); ++i) {
        checkerSamples[i] = pixelAt(14 + i, 16)[0];
    }

    material.opacityTexture = ArtifactCore::UniString(
        QString::fromStdString(averagePath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("opacity-minified-mip-selection"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    for (Uint32 i = 0; i < checkerSamples.size(); ++i) {
        const float averageSample = pixelAt(14 + i, 16)[0];
        EXPECT_NEAR(checkerSamples[i], averageSample, 0.025f)
            << "minified opacity checker sample " << i
            << " should match its average-alpha mip";
    }

    const auto traceObliqueMipSample = [&](const std::filesystem::path& path) {
        material.opacityTexture = ArtifactCore::UniString(
            QString::fromStdString(path.string()));
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("opacity-minified-mip-selection"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ArtifactCore::RTRayCamera camera;
        camera.origin[0] = 1.5f;
        camera.origin[2] = -1.0f;
        camera.target[2] = 0.0f;
        camera.verticalFovDegrees = 60.0f;
        EXPECT_TRUE(manager_->traceCamera(context_.RawPtr(),
                                          kRayTraceTestImageSize,
                                          kRayTraceTestImageSize, camera));
        return pixelAt(16, 16)[0];
    };
    const float obliqueCheckerSample = traceObliqueMipSample(checkerPath);
    const float obliqueAverageSample = traceObliqueMipSample(averagePath);
    EXPECT_NEAR(obliqueCheckerSample, obliqueAverageSample, 0.025f)
        << "an oblique camera should resolve the minified checker to its "
           "average-alpha mip";
}

TEST_F(RayTracingAcceptanceTest, AppliesNormalTextureInPerTriangleTangentSpace) {
    constexpr std::array<float, 12> vertices{
        -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f,
         0.9f,  0.9f, 0.0f, -0.9f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    constexpr std::array<float, 8> uvs{0.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 8> mirroredUvs1{1.0f, 0.0f, 0.0f, 0.0f,
                                                0.0f, 1.0f, 1.0f, 1.0f};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-normal-map", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-normal-map"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data(), nullptr,
        mirroredUvs1.data()));

    QTemporaryDir normalMapDirectory;
    ASSERT_TRUE(normalMapDirectory.isValid());
    const auto normalPath = std::filesystem::path(
        normalMapDirectory.filePath(QStringLiteral("normal-map.png"))
            .toStdString());
    QImage normalImage(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(normalImage.isNull());
    for (int y = 0; y < normalImage.height(); ++y) {
        auto* row = normalImage.scanLine(y);
        for (int x = 0; x < normalImage.width(); ++x) {
            auto* pixel = row + x * 4;
            const bool tilted = x >= 4;
            pixel[0] = tilted ? 255 : 128;
            pixel[1] = 128;
            pixel[2] = tilted ? 128 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(normalImage.save(QString::fromStdString(normalPath.string()), "PNG"));

    const auto moderateNormalPath = std::filesystem::path(
        normalMapDirectory.filePath(QStringLiteral("normal-map-moderate.png"))
            .toStdString());
    QImage moderateNormalImage(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(moderateNormalImage.isNull());
    for (int y = 0; y < moderateNormalImage.height(); ++y) {
        auto* row = moderateNormalImage.scanLine(y);
        for (int x = 0; x < moderateNormalImage.width(); ++x) {
            auto* pixel = row + x * 4;
            const bool tilted = x >= 4;
            pixel[0] = tilted ? 204 : 128;
            pixel[1] = 128;
            pixel[2] = tilted ? 230 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(moderateNormalImage.save(
        QString::fromStdString(moderateNormalPath.string()), "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.roughness = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto flatLeft = pixelAt(8, 16);
    const auto flatRight = pixelAt(24, 16);

    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(normalPath.string()));
    material.normalStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("24-pbr-normal-map.png"));
    const auto mappedLeft = pixelAt(8, 16);
    const auto mappedRight = pixelAt(24, 16);
    EXPECT_NEAR(mappedLeft[0], flatLeft[0], 0.03f);
    EXPECT_LT(mappedRight[0], flatRight[0] - 0.05f);

    QImage transparentNormalImage = normalImage;
    for (int y = 0; y < transparentNormalImage.height(); ++y) {
        auto* row = transparentNormalImage.scanLine(y);
        for (int x = 0; x < transparentNormalImage.width(); ++x) {
            row[x * 4 + 3] = 0;
        }
    }
    const auto transparentNormalPath = std::filesystem::path(
        normalMapDirectory.filePath(QStringLiteral("normal-map-alpha-zero.png"))
            .toStdString());
    ASSERT_TRUE(transparentNormalImage.save(
        QString::fromStdString(transparentNormalPath.string()), "PNG"));
    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(transparentNormalPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(pixelAt(8, 16)[channel], mappedLeft[channel], 0.01f)
            << "normal-map alpha should not change the RGB normal, left channel "
            << channel;
        EXPECT_NEAR(pixelAt(24, 16)[channel], mappedRight[channel], 0.01f)
            << "normal-map alpha should not change the RGB normal, right channel "
            << channel;
    }
    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(normalPath.string()));

    material.normalStrength = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto disabledRight = pixelAt(24, 16);
    EXPECT_NEAR(disabledRight[0], flatRight[0], 0.01f);

    material.normalStrength = 1.0f;
    material.normalUvSet = 1;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto uv1Left = pixelAt(8, 16);
    const auto uv1Right = pixelAt(24, 16);
    EXPECT_GT(std::fabs(uv1Left[0] - flatLeft[0]), 0.05f)
        << "UV1 mirroring should select the tilted normal texels on the left";
    EXPECT_NEAR(uv1Right[0], flatRight[0], 0.03f)
        << "UV1 mirroring should select the flat normal texels on the right";

    material.normalUvSet = 0;
    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(moderateNormalPath.string()));
    material.normalStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto fullStrengthRight = pixelAt(24, 16);

    QImage greenPerturbedNormalImage = moderateNormalImage;
    for (int y = 0; y < greenPerturbedNormalImage.height(); ++y) {
        auto* row = greenPerturbedNormalImage.scanLine(y);
        for (int x = greenPerturbedNormalImage.width() / 2;
             x < greenPerturbedNormalImage.width(); ++x) {
            row[x * 4 + 1] = 204;
        }
    }
    const auto greenPerturbedNormalPath = std::filesystem::path(
        normalMapDirectory.filePath(
            QStringLiteral("normal-map-green-perturbed.png")).toStdString());
    ASSERT_TRUE(greenPerturbedNormalImage.save(
        QString::fromStdString(greenPerturbedNormalPath.string()), "PNG"));
    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(greenPerturbedNormalPath.string()));
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto greenPerturbedRight = pixelAt(24, 16);
    const float greenChannelResponse = std::max(
        std::fabs(greenPerturbedRight[0] - fullStrengthRight[0]),
        std::max(std::fabs(greenPerturbedRight[1] - fullStrengthRight[1]),
                 std::fabs(greenPerturbedRight[2] - fullStrengthRight[2])));
    EXPECT_GT(greenChannelResponse, 0.001f)
        << "normal-map green should contribute to tangent-space shading";

    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(moderateNormalPath.string()));

    material.normalStrength = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto halfStrengthRight = pixelAt(24, 16);

    material.normalStrength = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto zeroStrengthRight = pixelAt(24, 16);
    const float fullStrengthDrop = flatRight[0] - fullStrengthRight[0];
    const float halfStrengthDrop = flatRight[0] - halfStrengthRight[0];
    EXPECT_GT(fullStrengthDrop, 0.02f);
    EXPECT_GT(halfStrengthDrop, 0.005f);
    EXPECT_LT(halfStrengthDrop, fullStrengthDrop - 0.005f);
    EXPECT_NEAR(zeroStrengthRight[0], flatRight[0], 0.01f);

    material.normalStrength = -0.5f;
    manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material);
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto negativeStrengthRight = pixelAt(24, 16);
    EXPECT_NEAR(negativeStrengthRight[0], zeroStrengthRight[0], 0.01f)
        << "negative normal strength should clamp to zero";

    material.normalStrength = std::numeric_limits<float>::quiet_NaN();
    manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material);
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto nonFiniteStrengthRight = pixelAt(24, 16);
    EXPECT_NEAR(nonFiniteStrengthRight[0], fullStrengthRight[0], 0.01f)
        << "non-finite normal strength should use its full-strength fallback";

    const auto rotatedNormalPath = std::filesystem::path(
        normalMapDirectory.filePath(QStringLiteral("normal-map-uniform.png"))
            .toStdString());
    QImage rotatedNormalImage(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(rotatedNormalImage.isNull());
    rotatedNormalImage.fill(QColor(204, 128, 230, 255));
    ASSERT_TRUE(rotatedNormalImage.save(
        QString::fromStdString(rotatedNormalPath.string()), "PNG"));
    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(rotatedNormalPath.string()));
    material.normalStrength = 1.0f;
    material.normalUvRotationDegrees = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unrotatedNormal = pixelAt(24, 16);

    material.normalUvRotationDegrees = 180.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rotatedNormal = pixelAt(24, 16);
    const float rotationResponse = std::max(
        std::fabs(unrotatedNormal[0] - rotatedNormal[0]),
        std::max(std::fabs(unrotatedNormal[1] - rotatedNormal[1]),
                 std::fabs(unrotatedNormal[2] - rotatedNormal[2])));
    EXPECT_GT(rotationResponse, 0.02f)
        << "rotating the normal UV frame should rotate the sampled tangent-space direction";

    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(normalPath.string()));
    material.normalUvRotationDegrees = 0.0f;
    material.normalUvOffsetU = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto unshiftedNormalLeft = pixelAt(8, 16);
    const auto unshiftedNormalRight = pixelAt(24, 16);

    material.normalUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto shiftedNormalLeft = pixelAt(8, 16);
    const auto shiftedNormalRight = pixelAt(24, 16);
    EXPECT_NEAR(shiftedNormalLeft[0], unshiftedNormalRight[0], 0.03f)
        << "normal-map U offset should move the tilted texels to the left";
    EXPECT_NEAR(shiftedNormalRight[0], unshiftedNormalLeft[0], 0.03f)
        << "normal-map U offset should wrap the flat texels onto the right";

    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(normalMapDirectory.filePath(
            QStringLiteral("missing-normal.png")).toStdString()));
    material.normalUvRotationDegrees = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto missingNormalRight = pixelAt(24, 16);
    EXPECT_NEAR(missingNormalRight[0], flatRight[0], 0.01f)
        << "a missing normal map should fall back to the geometric normal";

    material.normalTexture = ArtifactCore::UniString();
    material.normalStrength = 0.0f;
    material.clearcoat = 1.0f;
    material.clearcoatRoughness = 0.2f;
    material.clearcoatNormalStrength = 1.0f;
    const auto captureClearcoatNormalStrength = [this, &material](
                                                    float strength) {
        material.clearcoatNormalStrength = strength;
        EXPECT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-normal-map"), material));
        EXPECT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        EXPECT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        EXPECT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                            kRayTraceTestImageSize,
                                            kRayTraceTestImageSize));
        return std::array<std::array<float, 4>, 2>{
            pixelAt(8, 16), pixelAt(24, 16)};
    };
    material.clearcoatNormalTexture = ArtifactCore::UniString();
    material.clearcoatNormalUvRotationDegrees = 0.0f;
    material.clearcoatNormalUvScale = 1.0f;
    material.clearcoatNormalUvOffsetU = 0.0f;
    material.clearcoatNormalUvOffsetV = 0.0f;
    material.clearcoatNormalUvSet = 0;
    const auto geometricCoatNormalPixels = captureClearcoatNormalStrength(1.0f);
    const std::array<float, 3> savedBaseColor{
        material.baseColor[0], material.baseColor[1], material.baseColor[2]};
    const float savedMetallic = material.metallic;
    const float savedSpecular = material.specular;
    material.baseColor[0] = material.baseColor[1] =
        material.baseColor[2] = 0.0f;
    material.metallic = 0.0f;
    material.specular = 0.0f;
    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(moderateNormalPath.string()));
    material.normalUvRotationDegrees = 0.0f;
    material.normalUvScale = 1.0f;
    material.normalUvOffsetU = 0.0f;
    material.normalUvOffsetV = 0.0f;
    material.normalStrength = 0.0f;
    const auto zeroBaseNormalStrengthFallbackPixels =
        captureClearcoatNormalStrength(1.0f);
    material.normalStrength = 1.0f;
    const auto baseNormalFallbackCoatPixels =
        captureClearcoatNormalStrength(1.0f);
    for (std::size_t sampleIndex = 0; sampleIndex < 2; ++sampleIndex) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(zeroBaseNormalStrengthFallbackPixels[sampleIndex][channel],
                        baseNormalFallbackCoatPixels[sampleIndex][channel],
                        0.01f)
                << "clearcoat-normal fallback should use coat strength "
                   "independently of base normal strength, sample "
                << sampleIndex << ", channel " << channel;
        }
    }
    material.clearcoatNormalTexture = ArtifactCore::UniString(
        QString::fromStdString(moderateNormalPath.string()));
    const auto explicitCoatNormalWithBaseMapPixels =
        captureClearcoatNormalStrength(1.0f);
    for (std::size_t sampleIndex = 0; sampleIndex < 2; ++sampleIndex) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(baseNormalFallbackCoatPixels[sampleIndex][channel],
                        explicitCoatNormalWithBaseMapPixels[sampleIndex][channel],
                        0.01f)
                << "the base normal fallback should match the same explicit "
                   "coat-normal map, sample "
                << sampleIndex << ", channel " << channel;
        }
    }
    material.baseColor[0] = savedBaseColor[0];
    material.baseColor[1] = savedBaseColor[1];
    material.baseColor[2] = savedBaseColor[2];
    material.metallic = savedMetallic;
    material.specular = savedSpecular;
    material.normalTexture = ArtifactCore::UniString();
    material.normalStrength = 0.0f;
    const auto clearcoatNormalUv0Pixels = captureClearcoatNormalStrength(1.0f);
    const auto& clearcoatNormalUv0Left = clearcoatNormalUv0Pixels[0];
    const auto& clearcoatNormalUv0Right = clearcoatNormalUv0Pixels[1];
    EXPECT_GT(std::fabs(clearcoatNormalUv0Left[0] -
                        clearcoatNormalUv0Right[0]), 0.02f)
        << "the clearcoat-normal map should distinguish the UV0 halves";
    EXPECT_GT(std::fabs(clearcoatNormalUv0Right[0] -
                        geometricCoatNormalPixels[1][0]), 0.01f)
        << "full clearcoat-normal strength should perturb the geometric normal";

    material.normalTexture = ArtifactCore::UniString(
        QString::fromStdString(moderateNormalPath.string()));
    material.normalStrength = 1.0f;
    const auto clearcoatWithBaseNormalPixels =
        captureClearcoatNormalStrength(1.0f);
    for (std::size_t sampleIndex = 0; sampleIndex < 2; ++sampleIndex) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(clearcoatWithBaseNormalPixels[sampleIndex][channel],
                        clearcoatNormalUv0Pixels[sampleIndex][channel], 0.01f)
                << "changing the base normal map should preserve independent "
                   "clearcoat shading, sample "
                << sampleIndex << ", channel " << channel;
        }
    }
    material.normalTexture = ArtifactCore::UniString();
    material.normalStrength = 0.0f;

    QImage greenPerturbedClearcoatNormal = moderateNormalImage;
    for (int y = 0; y < greenPerturbedClearcoatNormal.height(); ++y) {
        auto* row = greenPerturbedClearcoatNormal.scanLine(y);
        for (int x = greenPerturbedClearcoatNormal.width() / 2;
             x < greenPerturbedClearcoatNormal.width(); ++x) {
            row[x * 4 + 1] = 204;
        }
    }
    const auto greenPerturbedClearcoatNormalPath = std::filesystem::path(
        normalMapDirectory.filePath(
            QStringLiteral("clearcoat-normal-green-perturbed.png"))
            .toStdString());
    ASSERT_TRUE(greenPerturbedClearcoatNormal.save(
        QString::fromStdString(greenPerturbedClearcoatNormalPath.string()),
        "PNG"));
    material.clearcoatNormalTexture = ArtifactCore::UniString(
        QString::fromStdString(greenPerturbedClearcoatNormalPath.string()));
    const auto greenPerturbedClearcoatPixels =
        captureClearcoatNormalStrength(1.0f);
    const auto& greenPerturbedClearcoatRight =
        greenPerturbedClearcoatPixels[1];
    const float clearcoatNormalGreenResponse = std::max(
        std::fabs(greenPerturbedClearcoatRight[0] - clearcoatNormalUv0Right[0]),
        std::max(
            std::fabs(greenPerturbedClearcoatRight[1] - clearcoatNormalUv0Right[1]),
            std::fabs(greenPerturbedClearcoatRight[2] - clearcoatNormalUv0Right[2])));
    EXPECT_GT(clearcoatNormalGreenResponse, 0.001f)
        << "clearcoat-normal green should contribute to coat tangent-space shading";

    QImage transparentClearcoatNormal = moderateNormalImage;
    for (int y = 0; y < transparentClearcoatNormal.height(); ++y) {
        auto* row = transparentClearcoatNormal.scanLine(y);
        for (int x = 0; x < transparentClearcoatNormal.width(); ++x) {
            row[x * 4 + 3] = 0;
        }
    }
    const auto transparentClearcoatNormalPath = std::filesystem::path(
        normalMapDirectory.filePath(
            QStringLiteral("clearcoat-normal-alpha-zero.png"))
            .toStdString());
    ASSERT_TRUE(transparentClearcoatNormal.save(
        QString::fromStdString(transparentClearcoatNormalPath.string()), "PNG"));
    material.clearcoatNormalTexture = ArtifactCore::UniString(
        QString::fromStdString(transparentClearcoatNormalPath.string()));
    const auto transparentClearcoatNormalPixels =
        captureClearcoatNormalStrength(1.0f);
    for (std::size_t sampleIndex = 0; sampleIndex < 2; ++sampleIndex) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(transparentClearcoatNormalPixels[sampleIndex][channel],
                        clearcoatNormalUv0Pixels[sampleIndex][channel], 0.01f)
                << "clearcoat-normal alpha should not change RGB shading, sample "
                << sampleIndex << ", channel " << channel;
        }
    }
    material.clearcoatNormalTexture = ArtifactCore::UniString(
        QString::fromStdString(moderateNormalPath.string()));

    const auto zeroClearcoatNormalStrength =
        captureClearcoatNormalStrength(0.0f);
    const auto halfClearcoatNormalStrength =
        captureClearcoatNormalStrength(0.5f);
    const auto aboveMaximumClearcoatNormalStrength =
        captureClearcoatNormalStrength(1.5f);
    const auto negativeClearcoatNormalStrength =
        captureClearcoatNormalStrength(-0.5f);
    const auto nonFiniteClearcoatNormalStrength =
        captureClearcoatNormalStrength(
            std::numeric_limits<float>::quiet_NaN());
    const float fullStrengthResponse = std::fabs(
        clearcoatNormalUv0Right[0] - geometricCoatNormalPixels[1][0]);
    const float halfStrengthResponse = std::fabs(
        halfClearcoatNormalStrength[1][0] - geometricCoatNormalPixels[1][0]);
    EXPECT_GT(halfStrengthResponse, 0.005f);
    EXPECT_LT(halfStrengthResponse, fullStrengthResponse - 0.005f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(zeroClearcoatNormalStrength[0][channel],
                    geometricCoatNormalPixels[0][channel], 0.01f)
            << "zero clearcoat-normal strength should restore the geometric normal";
        EXPECT_NEAR(zeroClearcoatNormalStrength[1][channel],
                    geometricCoatNormalPixels[1][channel], 0.01f)
            << "zero clearcoat-normal strength should restore the geometric normal";
        EXPECT_NEAR(negativeClearcoatNormalStrength[1][channel],
                    zeroClearcoatNormalStrength[1][channel], 0.01f)
            << "negative clearcoat-normal strength should clamp to zero";
        EXPECT_NEAR(aboveMaximumClearcoatNormalStrength[1][channel],
                    clearcoatNormalUv0Right[channel], 0.01f)
            << "clearcoat-normal strength above one should clamp to one";
        EXPECT_NEAR(nonFiniteClearcoatNormalStrength[1][channel],
                    clearcoatNormalUv0Right[channel], 0.01f)
            << "non-finite clearcoat-normal strength should use its full-strength fallback";
    }

    material.clearcoatNormalUvSet = 1;
    const auto clearcoatNormalUv1Pixels = captureClearcoatNormalStrength(1.0f);
    const auto& clearcoatNormalUv1Left = clearcoatNormalUv1Pixels[0];
    const auto& clearcoatNormalUv1Right = clearcoatNormalUv1Pixels[1];
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(clearcoatNormalUv1Left[channel],
                    clearcoatNormalUv0Right[channel], 0.035f)
            << "clearcoat-normal UV1 should mirror the left sample, channel "
            << channel;
        EXPECT_NEAR(clearcoatNormalUv1Right[channel],
                    clearcoatNormalUv0Left[channel], 0.035f)
            << "clearcoat-normal UV1 should mirror the right sample, channel "
            << channel;
    }

    material.clearcoatNormalUvSet = 0;
    material.clearcoatNormalUvOffsetU = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto shiftedClearcoatNormalLeft = pixelAt(8, 16);
    const auto shiftedClearcoatNormalRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(shiftedClearcoatNormalLeft[channel],
                    clearcoatNormalUv0Right[channel], 0.035f)
            << "clearcoat-normal U offset should move the tilted texels left, channel "
            << channel;
        EXPECT_NEAR(shiftedClearcoatNormalRight[channel],
                    clearcoatNormalUv0Left[channel], 0.035f)
            << "clearcoat-normal U offset should wrap the flat texels right, channel "
            << channel;
    }

    material.clearcoatNormalUvOffsetU = 0.0f;
    material.clearcoatNormalUvScale = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto scaledClearcoatNormalLeft = pixelAt(8, 16);
    const auto scaledClearcoatNormalRight = pixelAt(24, 16);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(scaledClearcoatNormalLeft[channel],
                    clearcoatNormalUv0Left[channel], 0.035f)
            << "clearcoat-normal scale should keep the left sample on the flat "
            << "half, channel " << channel;
        EXPECT_NEAR(scaledClearcoatNormalRight[channel],
                    clearcoatNormalUv0Left[channel], 0.035f)
            << "clearcoat-normal scale should repeat the flat half on the right, "
            << "channel " << channel;
    }
}

TEST_F(RayTracingAcceptanceTest,
       BaseAndClearcoatRoughnessUseMeshRendererEnvironmentPrefilter) {
    constexpr std::array<float, 9> triangleVertices{
        -0.8f, -0.8f, 0.0f, 0.8f, -0.8f, 0.0f, 0.0f, 0.8f, 0.0f};
    constexpr std::array<std::uint32_t, 3> triangleIndices{0, 1, 2};
    auto vertexBuffer = makeBuffer(
        triangleVertices.data(), sizeof(triangleVertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        triangleIndices.data(), sizeof(triangleIndices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("clearcoat-environment", vertexBuffer.RawPtr(), 3,
                indexBuffer.RawPtr(), 3, float4x4::Identity(), 1, 1,
                triangleVertices.data(), triangleIndices.data());

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = 0.0f;
    material.baseColor[1] = 0.0f;
    material.baseColor[2] = 0.0f;
    material.metallic = 1.0f;
    material.roughness = 0.5f;
    material.clearcoat = 1.0f;
    material.clearcoatRoughness = 0.04f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-environment"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));

    const char* configuredOutputDir = std::getenv("ARTIFACT_RT_FRAME_OUTPUT_DIR");
    const auto outputDir = configuredOutputDir && configuredOutputDir[0] != '\0'
        ? std::filesystem::path(configuredOutputDir)
        : std::filesystem::current_path() / "ray-tracing-frames";
    std::filesystem::create_directories(outputDir);
    const auto environmentPath = outputDir / "clearcoat-ibl-test-source.png";
    QImage environmentImage(32, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < environmentImage.height(); ++y) {
        auto* row = environmentImage.scanLine(y);
        for (int x = 0; x < environmentImage.width(); ++x) {
            auto* pixel = row + x * 4;
            const bool redHighlight = x >= 6 && x <= 9;
            pixel[0] = redHighlight ? 255 : 0;
            pixel[1] = 0;
            pixel[2] = redHighlight ? 0 : 255;
            pixel[3] = 255;
        }
    }
    ASSERT_TRUE(environmentImage.save(
        QString::fromStdString(environmentPath.string()), "PNG"));

    ArtifactCore::GpuContext gpuContext(device_.RawPtr(), context_.RawPtr());
    ArtifactCore::MeshRenderer meshRenderer(gpuContext);
    ASSERT_TRUE(meshRenderer.setEnvironmentMap(
        QString::fromStdString(environmentPath.string()), 8.0f));
    ASSERT_NE(meshRenderer.irradianceMapView(), nullptr);
    ASSERT_NE(meshRenderer.environmentMapView(), nullptr);
    ASSERT_NE(meshRenderer.sheenEnvironmentMapView(), nullptr);
    ASSERT_NE(meshRenderer.brdfLutView(), nullptr);
    manager_->setEnvironmentLighting(
        context_.RawPtr(), meshRenderer.irradianceMapView(),
        meshRenderer.environmentMapView(), meshRenderer.brdfLutView(), true,
        8.0f, 0.0f, meshRenderer.sheenEnvironmentMapView());
    ArtifactCore::RTSceneLight zeroIntensityLight;
    zeroIntensityLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    zeroIntensityLight.directionIntensity[2] = 1.0f;
    manager_->setSceneLights(context_.RawPtr(), &zeroIntensityLight, 1);

    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("19-clearcoat-ibl-smooth.png"));
    const auto smoothCoatPixel = pixelAt(16, 16);

    material.clearcoatRoughness = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-environment"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("20-clearcoat-ibl-rough.png"));
    const auto roughCoatPixel = pixelAt(16, 16);
    EXPECT_GT(smoothCoatPixel[0], roughCoatPixel[0] + 0.05f);
    EXPECT_GT(roughCoatPixel[2], smoothCoatPixel[2] + 0.05f);

    material.clearcoatRoughness = 0.5f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-environment"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto midRoughnessCoatPixel = pixelAt(16, 16);
    EXPECT_LT(midRoughnessCoatPixel[0], smoothCoatPixel[0] - 0.02f)
        << "intermediate clearcoat roughness should advance toward coarser "
           "environment mips";
    EXPECT_GT(midRoughnessCoatPixel[0], roughCoatPixel[0] + 0.02f)
        << "intermediate clearcoat roughness should retain some sharp-mip "
           "contribution";
    EXPECT_GT(midRoughnessCoatPixel[2], smoothCoatPixel[2] + 0.02f)
        << "intermediate clearcoat roughness should advance toward the "
           "coarse-mip environment color";
    EXPECT_LT(midRoughnessCoatPixel[2], roughCoatPixel[2] - 0.02f)
        << "intermediate clearcoat roughness should retain some sharp-mip "
           "contribution";

    material.clearcoat = 0.0f;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.8f;
    material.roughness = 0.04f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-environment"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto smoothBasePixel = pixelAt(16, 16);

    material.roughness = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("clearcoat-environment"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto roughBasePixel = pixelAt(16, 16);
    EXPECT_GT(smoothBasePixel[0], roughBasePixel[0] + 0.05f)
        << "smooth base roughness should sample the sharp environment mip";
    EXPECT_GT(roughBasePixel[2], smoothBasePixel[2] + 0.05f)
        << "rough base material should sample the coarser environment mip";
}

TEST_F(RayTracingAcceptanceTest, MeshRendererUsesSplitSumFresnelAndDiffuseEnergy) {
    constexpr Uint32 width = 65;
    constexpr Uint32 height = 65;
    constexpr std::array<float, 9> positions{
        -0.8f, -0.8f, 0.5f, 0.8f, -0.8f, 0.5f, 0.0f, 0.8f, 0.5f};
    constexpr float normalX = 0.8660254038f;
    constexpr std::array<float, 9> normals{
        normalX, 0.0f, -0.5f, normalX, 0.0f, -0.5f, normalX, 0.0f, -0.5f};
    constexpr std::array<float, 6> uvs{0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};

    ArtifactCore::GpuContext gpuContext(device_.RawPtr(), context_.RawPtr());
    ArtifactCore::MeshRenderer renderer(gpuContext);
    renderer.setRenderTargetFormat(TEX_FORMAT_RGBA32_FLOAT);
    renderer.initialize(1, 3, 3);
    renderer.updateMeshGeometry(positions.data(), normals.data(), uvs.data(),
                                indices.data());
    const std::array<float, 16> identity{
        1, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 1};
    renderer.setViewMatrix(identity.data());
    renderer.setProjectionMatrix(identity.data());

    ArtifactCore::InstanceData instance{};
    std::copy(identity.begin(), identity.end(), instance.transform);
    std::copy(identity.begin(), identity.end(), instance.previousTransform);
    instance.color[0] = 1.0f;
    instance.color[1] = 1.0f;
    instance.color[2] = 1.0f;
    instance.color[3] = 1.0f;
    instance.weight = 1.0f;
    renderer.updateInstanceData(&instance, 1);

    auto makeCube = [&](const char* name, const float (&color)[4]) {
        TextureDesc desc;
        desc.Name = name;
        desc.Type = RESOURCE_DIM_TEX_CUBE;
        desc.Width = 1;
        desc.Height = 1;
        desc.ArraySize = 6;
        desc.MipLevels = 1;
        desc.Format = TEX_FORMAT_RGBA32_FLOAT;
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = BIND_SHADER_RESOURCE;
        std::array<TextureSubResData, 6> faces{};
        for (auto& face : faces) {
            face.pData = color;
            face.Stride = sizeof(color);
        }
        TextureData data;
        data.pSubResources = faces.data();
        data.NumSubresources = static_cast<Uint32>(faces.size());
        RefCntAutoPtr<ITexture> texture;
        device_->CreateTexture(desc, &data, &texture);
        EXPECT_NE(texture, nullptr);
        if (!texture) return RefCntAutoPtr<ITextureView>{};
        RefCntAutoPtr<ITextureView> view;
        view = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        return view;
    };
    constexpr float blackCubeColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
    constexpr float specularCubeColor[4]{1.0f, 0.5f, 0.25f, 1.0f};
    auto irradianceView = makeCube("MeshRendererAcceptanceIrradiance",
                                   blackCubeColor);
    auto prefilteredView = makeCube("MeshRendererAcceptancePrefiltered",
                                    specularCubeColor);
    ASSERT_NE(irradianceView, nullptr);
    ASSERT_NE(prefilteredView, nullptr);

    constexpr Uint32 brdfLutWidth = 128;
    constexpr Uint32 brdfLutHeight = 64;
    std::vector<float> brdfValues(
        static_cast<size_t>(brdfLutWidth) * brdfLutHeight * 4, 0.0f);
    for (Uint32 y = 0; y < brdfLutHeight; ++y) {
        for (Uint32 x = 0; x < brdfLutWidth; ++x) {
            const size_t offset =
                (static_cast<size_t>(y) * brdfLutWidth + x) * 4;
            brdfValues[offset + 0] = x < brdfLutWidth / 2 ? 0.5f : 1.0f;
            brdfValues[offset + 1] = x < brdfLutWidth / 2 ? 0.25f : 0.0f;
            brdfValues[offset + 3] = 1.0f;
        }
    }
    TextureDesc brdfDesc;
    brdfDesc.Name = "MeshRendererAcceptanceBrdfLut";
    brdfDesc.Type = RESOURCE_DIM_TEX_2D;
    brdfDesc.Width = brdfLutWidth;
    brdfDesc.Height = brdfLutHeight;
    brdfDesc.MipLevels = 1;
    brdfDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    brdfDesc.Usage = USAGE_IMMUTABLE;
    brdfDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData brdfSubresource{};
    brdfSubresource.pData = brdfValues.data();
    brdfSubresource.Stride =
        static_cast<Uint64>(brdfLutWidth * 4 * sizeof(float));
    TextureData brdfData;
    brdfData.pSubResources = &brdfSubresource;
    brdfData.NumSubresources = 1;
    RefCntAutoPtr<ITexture> brdfTexture;
    device_->CreateTexture(brdfDesc, &brdfData, &brdfTexture);
    ASSERT_NE(brdfTexture, nullptr);
    auto brdfView = brdfTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(brdfView, nullptr);
    renderer.setEnvironmentMaps(prefilteredView, prefilteredView, brdfView, 1.0f);

    TextureDesc colorDesc;
    colorDesc.Name = "MeshRendererAcceptanceColorTarget";
    colorDesc.Type = RESOURCE_DIM_TEX_2D;
    colorDesc.Width = width;
    colorDesc.Height = height;
    colorDesc.MipLevels = 1;
    colorDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    colorDesc.Usage = USAGE_DEFAULT;
    colorDesc.BindFlags = BIND_RENDER_TARGET;
    RefCntAutoPtr<ITexture> colorTexture;
    device_->CreateTexture(colorDesc, nullptr, &colorTexture);
    ASSERT_NE(colorTexture, nullptr);
    auto colorView = colorTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    ASSERT_NE(colorView, nullptr);

    TextureDesc depthDesc;
    depthDesc.Name = "MeshRendererAcceptanceDepthTarget";
    depthDesc.Type = RESOURCE_DIM_TEX_2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.Format = TEX_FORMAT_D32_FLOAT;
    depthDesc.Usage = USAGE_DEFAULT;
    depthDesc.BindFlags = BIND_DEPTH_STENCIL;
    RefCntAutoPtr<ITexture> depthTexture;
    device_->CreateTexture(depthDesc, nullptr, &depthTexture);
    ASSERT_NE(depthTexture, nullptr);
    auto depthView = depthTexture->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    ASSERT_NE(depthView, nullptr);

    TextureDesc stagingDesc;
    stagingDesc.Name = "MeshRendererAcceptanceReadback";
    stagingDesc.Type = RESOURCE_DIM_TEX_2D;
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    stagingDesc.Usage = USAGE_STAGING;
    stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<ITexture> stagingTexture;
    device_->CreateTexture(stagingDesc, nullptr, &stagingTexture);
    ASSERT_NE(stagingTexture, nullptr);

    const auto renderPixelAt = [&](Uint32 sampleX, Uint32 sampleY) {
        const float clearColor[4]{0.0f, 0.0f, 0.0f, 0.0f};
        context_->SetRenderTargets(1, &colorView, depthView,
                                   RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->ClearRenderTarget(colorView, clearColor,
                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->ClearDepthStencil(depthView, CLEAR_DEPTH_FLAG, 1.0f, 0,
                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const Viewport viewport{0.0f, 0.0f, static_cast<float>(width),
                                static_cast<float>(height), 0.0f, 1.0f};
        context_->SetViewports(1, &viewport, width, height);
        renderer.prepare(context_.RawPtr());
        renderer.draw(context_.RawPtr(), 1);
        context_->SetRenderTargets(0, nullptr, nullptr,
                                   RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        CopyTextureAttribs copy(colorTexture,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                stagingTexture,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->CopyTexture(copy);
        context_->Flush();
        context_->WaitForIdle();
        MappedTextureSubresource mapped{};
        context_->MapTextureSubresource(stagingTexture, 0, 0, MAP_READ,
                                        MAP_FLAG_NONE, nullptr, mapped);
        EXPECT_NE(mapped.pData, nullptr);
        if (!mapped.pData) return std::array<float, 4>{};
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData) +
                          sampleY * mapped.Stride;
        std::array<float, 4> pixel{};
        std::memcpy(pixel.data(), row + sampleX * sizeof(pixel),
                    sizeof(pixel));
        context_->UnmapTextureSubresource(stagingTexture, 0, 0);
        return pixel;
    };
    const auto renderCenterPixel = [&]() {
        return renderPixelAt(width / 2, height / 2);
    };

    instance.color[0] = 0.0f;
    instance.color[1] = 0.0f;
    instance.color[2] = 0.0f;
    renderer.updateInstanceData(&instance, 1);
    renderer.setPbrFactors(0.0f, 0.5f, 1.0f, 1.0f);
    renderer.setPrincipledFactors(0.5f, 1.5f, 0.0f, 0.0f, 0.04f);
    const auto specularPixel = renderCenterPixel();
    // NdotV is 0.5, F0 is 0.04, and the LUT stores A=0.5 / B=0.25.
    // The split-sum result is F0*A+B = 0.27. Applying Schlick to A again
    // would incorrectly produce 0.285 for this grazing-angle sample.
    EXPECT_NEAR(specularPixel[0], 0.27f, 0.004f);
    EXPECT_NEAR(specularPixel[1], 0.135f, 0.004f);
    EXPECT_NEAR(specularPixel[2], 0.0675f, 0.004f);

    QTemporaryDir aoInputDirectory;
    ASSERT_TRUE(aoInputDirectory.isValid());
    constexpr std::array<float, 4> aoTestRoughnesses{0.04f, 0.2f, 0.5f, 0.9f};
    constexpr std::array<int, 5> aoTestValues{0, 64, 128, 192, 255};
    std::array<std::array<float, aoTestValues.size()>,
               aoTestRoughnesses.size()> reflectionRetention{};
    const auto expectedSpecularOcclusion = [](float visibility, float roughness) {
        const float exponent = std::exp2(-16.0f * roughness - 1.0f);
        return std::clamp(std::pow(0.5f + visibility, exponent) -
                              1.0f + visibility,
                          0.0f, 1.0f);
    };
    renderer.setOcclusionTexture(QString{});
    for (size_t roughnessIndex = 0;
         roughnessIndex < aoTestRoughnesses.size(); ++roughnessIndex) {
        const float roughness = aoTestRoughnesses[roughnessIndex];
        renderer.setPbrFactors(0.0f, roughness, 1.0f, 1.0f);
        const auto unoccludedPixel = renderCenterPixel();
        float previousRetention = -1.0f;
        for (size_t aoIndex = 0; aoIndex < aoTestValues.size(); ++aoIndex) {
            const int aoValue = aoTestValues[aoIndex];
            QImage aoImage(1, 1, QImage::Format_Grayscale8);
            aoImage.fill(aoValue);
            const auto aoTexturePath = aoInputDirectory.filePath(
                QStringLiteral("mesh-renderer-ao-%1.png").arg(aoValue));
            ASSERT_TRUE(aoImage.save(aoTexturePath, "PNG"));
            renderer.setOcclusionTexture(aoTexturePath);
            const auto occludedPixel = renderCenterPixel();
            const float retention = occludedPixel[0] /
                std::max(unoccludedPixel[0], 1.0e-5f);
            const float visibility = static_cast<float>(aoValue) / 255.0f;
            reflectionRetention[roughnessIndex][aoIndex] = retention;
            EXPECT_NEAR(retention,
                        expectedSpecularOcclusion(visibility, roughness),
                        0.03f)
                << "AO=" << aoValue << " roughness=" << roughness;
            EXPECT_GE(retention, previousRetention - 0.01f)
                << "AO response must be monotonic at roughness=" << roughness;
            previousRetention = retention;
        }
    }
    for (size_t aoIndex = 0; aoIndex < aoTestValues.size(); ++aoIndex) {
        const float visibility =
            static_cast<float>(aoTestValues[aoIndex]) / 255.0f;
        const bool roughnessShouldIncreaseRetention = 0.5f + visibility < 1.0f;
        for (size_t roughnessIndex = 1;
             roughnessIndex < aoTestRoughnesses.size(); ++roughnessIndex) {
            if (roughnessShouldIncreaseRetention) {
                EXPECT_GE(reflectionRetention[roughnessIndex][aoIndex],
                          reflectionRetention[roughnessIndex - 1][aoIndex] -
                              0.01f)
                    << "Roughness response at AO=" << aoTestValues[aoIndex];
            } else {
                EXPECT_LE(reflectionRetention[roughnessIndex][aoIndex],
                          reflectionRetention[roughnessIndex - 1][aoIndex] +
                              0.01f)
                    << "Roughness response at AO=" << aoTestValues[aoIndex];
            }
        }
    }
    renderer.setOcclusionTexture(QString{});

    QImage spatialAoImage(4, 1, QImage::Format_Grayscale8);
    constexpr std::array<std::uint8_t, 4> spatialAoValues{32, 96, 160, 224};
    std::memcpy(spatialAoImage.scanLine(0), spatialAoValues.data(),
                spatialAoValues.size());
    const auto spatialAoTexturePath = aoInputDirectory.filePath(
        QStringLiteral("mesh-renderer-spatial-ao.png"));
    ASSERT_TRUE(spatialAoImage.save(spatialAoTexturePath, "PNG"));
    renderer.setPbrFactors(0.0f, 0.2f, 1.0f, 1.0f);
    const auto leftUnoccluded = renderPixelAt(20, height / 2);
    const auto rightUnoccluded = renderPixelAt(44, height / 2);
    renderer.setOcclusionTexture(spatialAoTexturePath);
    const auto leftSpatiallyOccluded = renderPixelAt(20, height / 2);
    const auto rightSpatiallyOccluded = renderPixelAt(44, height / 2);
    const float leftSpatialRetention = leftSpatiallyOccluded[0] /
        std::max(leftUnoccluded[0], 1.0e-5f);
    const float rightSpatialRetention = rightSpatiallyOccluded[0] /
        std::max(rightUnoccluded[0], 1.0e-5f);
    EXPECT_LT(leftSpatialRetention, rightSpatialRetention);
    renderer.setOcclusionTexture(QString{});

    renderer.setUvTransform(0.5f, 0.0f, 1.0f, 1.0f, 0.0f);
    const auto shiftedLeftUnoccluded = renderPixelAt(20, height / 2);
    const auto shiftedRightUnoccluded = renderPixelAt(44, height / 2);
    renderer.setOcclusionTexture(spatialAoTexturePath);
    const auto shiftedLeftOccluded = renderPixelAt(20, height / 2);
    const auto shiftedRightOccluded = renderPixelAt(44, height / 2);
    const float shiftedLeftRetention = shiftedLeftOccluded[0] /
        std::max(shiftedLeftUnoccluded[0], 1.0e-5f);
    const float shiftedRightRetention = shiftedRightOccluded[0] /
        std::max(shiftedRightUnoccluded[0], 1.0e-5f);
    EXPECT_GT(shiftedLeftRetention, shiftedRightRetention);
    renderer.setOcclusionTexture(QString{});
    renderer.setUvTransform(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

    instance.color[0] = 1.0f;
    instance.color[1] = 1.0f;
    instance.color[2] = 1.0f;
    renderer.updateInstanceData(&instance, 1);
    renderer.setEnvironmentMaps(prefilteredView, prefilteredView, brdfView, 1.0f);
    renderer.setPbrFactors(0.0f, 0.5f, 1.0f, 1.0f);
    renderer.setPrincipledFactors(0.5f, 1.5f, 0.0f, 0.0f, 0.04f);
    const auto diffusePixel = renderCenterPixel();
    EXPECT_NEAR(diffusePixel[0], 1.47f, 0.008f);

    renderer.setEnvironmentMaps(irradianceView, irradianceView, brdfView, 1.0f);
    renderer.setPrincipledFactors(0.5f, 1.5f, 1.0f, 0.0f, 0.04f);
    const auto transmittedPixel = renderCenterPixel();
    EXPECT_NEAR(transmittedPixel[0], 0.24f, 0.008f);

    const auto hdrPath = aoInputDirectory.filePath(
        QStringLiteral("mesh-renderer-hdr-source.hdr"));
    QFile hdrFile(hdrPath);
    ASSERT_TRUE(hdrFile.open(QIODevice::WriteOnly));
    constexpr char hdrHeader[] =
        "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 4 +X 8\n";
    ASSERT_EQ(hdrFile.write(hdrHeader, sizeof(hdrHeader) - 1),
              static_cast<qint64>(sizeof(hdrHeader) - 1));
    constexpr std::array<std::uint8_t, 4> rgbePixel{128, 32, 8, 131};
    for (int y = 0; y < 4; ++y) {
        constexpr std::array<std::uint8_t, 4> scanlineHeader{2, 2, 0, 8};
        ASSERT_EQ(hdrFile.write(
                      reinterpret_cast<const char*>(scanlineHeader.data()),
                      static_cast<qint64>(scanlineHeader.size())),
                  static_cast<qint64>(scanlineHeader.size()));
        for (std::uint8_t channel : rgbePixel) {
            const std::array<std::uint8_t, 2> run{136, channel};
            ASSERT_EQ(hdrFile.write(reinterpret_cast<const char*>(run.data()),
                                    static_cast<qint64>(run.size())),
                      static_cast<qint64>(run.size()));
        }
    }
    ASSERT_TRUE(hdrFile.flush());
    hdrFile.close();

    ArtifactCore::ImageImporter hdrImporter;
    ASSERT_TRUE(hdrImporter.open(hdrPath));
    const auto hdrImage = hdrImporter.readImage();
    ASSERT_TRUE(hdrImage.isValid());
    EXPECT_TRUE(hdrImage.isHDR);
    EXPECT_EQ(hdrImage.pixelType, QStringLiteral("float"));
    std::array<float, 4> decodedHdrPixel{};
    std::memcpy(decodedHdrPixel.data(), hdrImage.data.constData(),
                sizeof(decodedHdrPixel));
    EXPECT_NEAR(decodedHdrPixel[0], 4.0f, 0.01f);
    EXPECT_NEAR(decodedHdrPixel[1], 1.0f, 0.01f);
    EXPECT_NEAR(decodedHdrPixel[2], 0.25f, 0.01f);

    ASSERT_TRUE(renderer.setEnvironmentMap(hdrPath, 1.0f));
    ASSERT_TRUE(renderer.hasEnvironmentMap());
    instance.color[0] = 1.0f;
    instance.color[1] = 0.0f;
    instance.color[2] = 0.0f;
    renderer.updateInstanceData(&instance, 1);
    renderer.setPbrFactors(1.0f, 0.04f, 1.0f, 1.0f);
    renderer.setPrincipledFactors(0.5f, 1.5f, 0.0f, 0.0f, 0.04f);
    const auto hdrLitPixel = renderCenterPixel();
    ASSERT_TRUE(renderer.setEnvironmentMap(hdrPath, 0.25f));
    const auto quarterIntensityPixel = renderCenterPixel();
    EXPECT_GT(hdrLitPixel[0], 2.0f);
    EXPECT_NEAR(hdrLitPixel[0] - 0.24f,
                (quarterIntensityPixel[0] - 0.24f) * 4.0f, 0.02f);

    const auto projectRoot = std::filesystem::path(__FILE__)
                                 .parent_path().parent_path().parent_path();
    const auto studioHdrPath = projectRoot /
        "libs/openusd/pxr/imaging/hdx/textures/StinsonBeach.hdr";
    ASSERT_TRUE(std::filesystem::exists(studioHdrPath))
        << "Expected the vendored OpenUSD StinsonBeach HDR test image";
    const auto studioHdrPathString =
        QString::fromStdString(studioHdrPath.string());
    ArtifactCore::ImageImporter studioHdrImporter;
    ASSERT_TRUE(studioHdrImporter.open(studioHdrPathString));
    const auto studioHdrImage = studioHdrImporter.readImage();
    ASSERT_TRUE(studioHdrImage.isValid());
    EXPECT_TRUE(studioHdrImage.isHDR);
    EXPECT_EQ(studioHdrImage.pixelType, QStringLiteral("float"));
    bool containsRadianceAboveDisplayWhite = false;
    for (int pixelIndex = 0;
         pixelIndex < studioHdrImage.width * studioHdrImage.height;
         ++pixelIndex) {
        std::array<float, 4> sourcePixel{};
        std::memcpy(sourcePixel.data(),
                    studioHdrImage.data.constData() +
                        pixelIndex * static_cast<int>(sizeof(sourcePixel)),
                    sizeof(sourcePixel));
        if (std::max(sourcePixel[0],
                     std::max(sourcePixel[1], sourcePixel[2])) > 1.0f) {
            containsRadianceAboveDisplayWhite = true;
            break;
        }
    }
    EXPECT_TRUE(containsRadianceAboveDisplayWhite);
    const auto environmentGenerationStart = std::chrono::steady_clock::now();
    ASSERT_TRUE(renderer.setEnvironmentMap(studioHdrPathString, 1.0f));
    const auto environmentGenerationElapsed =
        std::chrono::steady_clock::now() - environmentGenerationStart;
    const auto environmentGenerationMilliseconds =
        std::chrono::duration<double, std::milli>(environmentGenerationElapsed)
            .count();
    std::fprintf(stderr,
                 "PBR_ENV_GENERATION StinsonBeach %dx%d %.3f ms (CPU call; GPU completion excluded)\n",
                 studioHdrImage.width, studioHdrImage.height,
                 environmentGenerationMilliseconds);
    auto* generatedBrdfView = renderer.brdfLutView();
    ASSERT_NE(generatedBrdfView, nullptr);
    auto* generatedBrdfTexture = generatedBrdfView->GetTexture();
    ASSERT_NE(generatedBrdfTexture, nullptr);
    const auto& generatedBrdfDesc = generatedBrdfTexture->GetDesc();
    ASSERT_EQ(generatedBrdfDesc.Width, 128u);
    ASSERT_EQ(generatedBrdfDesc.Height, 64u);
    ASSERT_EQ(generatedBrdfDesc.Format, TEX_FORMAT_RGBA32_FLOAT);
    TextureDesc brdfStagingDesc;
    brdfStagingDesc.Name = "GeneratedBrdfLutReadback";
    brdfStagingDesc.Type = RESOURCE_DIM_TEX_2D;
    brdfStagingDesc.Width = generatedBrdfDesc.Width;
    brdfStagingDesc.Height = generatedBrdfDesc.Height;
    brdfStagingDesc.MipLevels = 1;
    brdfStagingDesc.Format = generatedBrdfDesc.Format;
    brdfStagingDesc.Usage = USAGE_STAGING;
    brdfStagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<ITexture> brdfStagingTexture;
    device_->CreateTexture(brdfStagingDesc, nullptr, &brdfStagingTexture);
    ASSERT_NE(brdfStagingTexture, nullptr);
    CopyTextureAttribs brdfCopy(
        generatedBrdfTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        brdfStagingTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context_->CopyTexture(brdfCopy);
    context_->Flush();
    context_->WaitForIdle();
    MappedTextureSubresource brdfMapped{};
    context_->MapTextureSubresource(brdfStagingTexture, 0, 0, MAP_READ,
                                    MAP_FLAG_NONE, nullptr, brdfMapped);
    ASSERT_NE(brdfMapped.pData, nullptr);
    constexpr std::array<Uint32, 3> viewSamples{4u, 32u, 60u};
    constexpr std::array<Uint32, 3> roughnessSamples{4u, 22u, 54u};
    std::array<std::array<float, 3>, roughnessSamples.size()> singleScatterEnergy{};
    std::array<std::array<std::array<float, 2>, viewSamples.size()>,
               roughnessSamples.size()> splitSumValues{};
    for (size_t roughnessIndex = 0;
         roughnessIndex < roughnessSamples.size(); ++roughnessIndex) {
        const auto* row = static_cast<const std::uint8_t*>(brdfMapped.pData) +
            roughnessSamples[roughnessIndex] * brdfMapped.Stride;
        for (size_t viewIndex = 0; viewIndex < viewSamples.size(); ++viewIndex) {
            std::array<float, 4> lutValue{};
            std::memcpy(lutValue.data(), row +
                viewSamples[viewIndex] * sizeof(lutValue), sizeof(lutValue));
            EXPECT_TRUE(std::isfinite(lutValue[0]));
            EXPECT_TRUE(std::isfinite(lutValue[1]));
            EXPECT_GE(lutValue[0], 0.0f);
            EXPECT_GE(lutValue[1], 0.0f);
            splitSumValues[roughnessIndex][viewIndex] =
                {lutValue[0], lutValue[1]};
            singleScatterEnergy[roughnessIndex][viewIndex] =
                lutValue[0] + lutValue[1];
            EXPECT_LE(singleScatterEnergy[roughnessIndex][viewIndex], 1.05f);
        }
        std::fprintf(stderr,
            "PBR_BRDF_RESPONSE roughnessRow=%u NdotVRows=%u,%u,%u energy=%.5f,%.5f,%.5f\n",
            roughnessSamples[roughnessIndex], viewSamples[0], viewSamples[1],
            viewSamples[2], singleScatterEnergy[roughnessIndex][0],
            singleScatterEnergy[roughnessIndex][1],
            singleScatterEnergy[roughnessIndex][2]);
    }
    context_->UnmapTextureSubresource(brdfStagingTexture, 0, 0);
    EXPECT_GT(std::fabs(singleScatterEnergy[2][0] -
                        singleScatterEnergy[2][2]), 0.02f)
        << "Rough GGX LUT should preserve view-angle response";
    EXPECT_GT(singleScatterEnergy[0][1], singleScatterEnergy[2][1]);
    const auto integrateUniformHemisphereReference = [](
        Uint32 viewColumn, Uint32 roughnessRow) {
        constexpr int muSamples = 256;
        constexpr int phiSamples = 512;
        constexpr float pi = 3.14159265358979323846f;
        const float roughness = (static_cast<float>(roughnessRow) + 0.5f) / 64.0f;
        const float alpha = roughness * roughness;
        const float alphaSquared = alpha * alpha;
        const float nDotV = (static_cast<float>(viewColumn) + 0.5f) / 64.0f;
        const float viewX = std::sqrt(std::max(0.0f, 1.0f - nDotV * nDotV));
        const float geometryK = (roughness + 1.0f) *
                                (roughness + 1.0f) * 0.125f;
        const float geometryV = nDotV / std::max(
            nDotV * (1.0f - geometryK) + geometryK, 1.0e-6f);
        float scaleIntegral = 0.0f;
        float biasIntegral = 0.0f;
        for (int muIndex = 0; muIndex < muSamples; ++muIndex) {
            const float nDotL = (static_cast<float>(muIndex) + 0.5f) /
                               static_cast<float>(muSamples);
            const float lightSinTheta = std::sqrt(
                std::max(0.0f, 1.0f - nDotL * nDotL));
            const float geometryL = nDotL / std::max(
                nDotL * (1.0f - geometryK) + geometryK, 1.0e-6f);
            for (int phiIndex = 0; phiIndex < phiSamples; ++phiIndex) {
                const float phi = 2.0f * pi *
                    (static_cast<float>(phiIndex) + 0.5f) /
                    static_cast<float>(phiSamples);
                const float viewDotLight = nDotV * nDotL +
                    viewX * lightSinTheta * std::cos(phi);
                const float halfLength = std::sqrt(std::max(
                    1.0e-8f, 2.0f + 2.0f * viewDotLight));
                const float nDotH = std::clamp(
                    (nDotV + nDotL) / halfLength, 0.0f, 1.0f);
                const float viewDotHalf = std::clamp(
                    (1.0f + viewDotLight) / halfLength, 0.0f, 1.0f);
                const float distributionDenominator =
                    nDotH * nDotH * (alphaSquared - 1.0f) + 1.0f;
                const float distribution = alphaSquared /
                    (pi * distributionDenominator * distributionDenominator);
                const float visibility = distribution * geometryV * geometryL /
                    std::max(4.0f * nDotV, 1.0e-6f);
                const float fresnel = std::pow(1.0f - viewDotHalf, 5.0f);
                scaleIntegral += visibility * (1.0f - fresnel);
                biasIntegral += visibility * fresnel;
            }
        }
        const float solidAnglePerSample = 2.0f * pi /
            static_cast<float>(muSamples * phiSamples);
        return std::array<float, 2>{scaleIntegral * solidAnglePerSample,
                                    biasIntegral * solidAnglePerSample};
    };
    const auto integrateLowRoughnessSlopeQuadratureReference = [](
        Uint32 viewColumn, Uint32 roughnessRow, int polarSamples,
        int azimuthSamples) {
        constexpr double pi = 3.14159265358979323846;
        constexpr int maximumPolarSamples = 96;
        std::array<double, maximumPolarSamples> nodes{};
        std::array<double, maximumPolarSamples> weights{};
        const auto fillGaussLegendre = [](int count, auto& outputNodes,
                                          auto& outputWeights) {
            constexpr double rootTolerance = 2.0e-15;
            const double piValue = 3.14159265358979323846;
            for (int index = 0; index < (count + 1) / 2; ++index) {
                double root = std::cos(
                    piValue * (static_cast<double>(index) + 0.75) /
                    (static_cast<double>(count) + 0.5));
                double derivative = 0.0;
                for (int iteration = 0; iteration < 32; ++iteration) {
                    double previous = 1.0;
                    double polynomial = root;
                    for (int order = 2; order <= count; ++order) {
                        const double next =
                            ((2.0 * order - 1.0) * root * polynomial -
                             (order - 1.0) * previous) / order;
                        previous = polynomial;
                        polynomial = next;
                    }
                    if (count == 1) {
                        previous = 1.0;
                        polynomial = root;
                    }
                    derivative = count * (root * polynomial - previous) /
                                 (root * root - 1.0);
                    const double nextRoot = root - polynomial / derivative;
                    if (std::fabs(nextRoot - root) <= rootTolerance) {
                        root = nextRoot;
                        break;
                    }
                    root = nextRoot;
                }
                double previous = 1.0;
                double polynomial = root;
                for (int order = 2; order <= count; ++order) {
                    const double next =
                        ((2.0 * order - 1.0) * root * polynomial -
                         (order - 1.0) * previous) / order;
                    previous = polynomial;
                    polynomial = next;
                }
                if (count == 1) {
                    previous = 1.0;
                    polynomial = root;
                }
                derivative = count * (root * polynomial - previous) /
                             (root * root - 1.0);
                const double weight = 2.0 /
                    ((1.0 - root * root) * derivative * derivative);
                outputNodes[index] = -root;
                outputNodes[count - 1 - index] = root;
                outputWeights[index] = weight;
                outputWeights[count - 1 - index] = weight;
            }
        };
        fillGaussLegendre(polarSamples, nodes, weights);

        const double roughness =
            (static_cast<double>(roughnessRow) + 0.5) / 64.0;
        const double alpha = roughness * roughness;
        const double nDotV =
            (static_cast<double>(viewColumn) + 0.5) / 64.0;
        const double viewX = std::sqrt(std::max(0.0, 1.0 - nDotV * nDotV));
        const double geometryK = (roughness + 1.0) *
                                 (roughness + 1.0) * 0.125;
        const double geometryV = nDotV / std::max(
            nDotV * (1.0 - geometryK) + geometryK, 1.0e-6);
        double scaleIntegral = 0.0;
        double biasIntegral = 0.0;
        for (int polarIndex = 0; polarIndex < polarSamples; ++polarIndex) {
            const double u = (nodes[polarIndex] + 1.0) * pi * 0.25;
            const double quadratureWeight = weights[polarIndex] * pi * 0.25;
            const double normalizedSlope = std::tan(u);
            const double denominator = std::sqrt(
                1.0 + alpha * alpha * normalizedSlope * normalizedSlope);
            const double nDotH = 1.0 / denominator;
            const double sinTheta = alpha * normalizedSlope / denominator;
            const double radialWeight = 2.0 * quadratureWeight *
                std::sin(u) * std::cos(u) / azimuthSamples;
            for (int azimuthIndex = 0; azimuthIndex < azimuthSamples;
                 ++azimuthIndex) {
                const double phi = 2.0 * pi *
                    (static_cast<double>(azimuthIndex) + 0.5) /
                    azimuthSamples;
                const double viewDotHalf = std::max(
                    viewX * sinTheta * std::cos(phi) + nDotV * nDotH, 0.0);
                const double nDotL = 2.0 * viewDotHalf * nDotH - nDotV;
                if (nDotL <= 0.0) continue;
                const double geometryL = nDotL / std::max(
                    nDotL * (1.0 - geometryK) + geometryK, 1.0e-6);
                const double visibility = geometryV * geometryL * viewDotHalf /
                    std::max(nDotH * nDotV, 1.0e-6);
                const double fresnel = std::pow(1.0 - viewDotHalf, 5.0);
                scaleIntegral += radialWeight * visibility * (1.0 - fresnel);
                biasIntegral += radialWeight * visibility * fresnel;
            }
        }
        return std::array<float, 2>{static_cast<float>(scaleIntegral),
                                    static_cast<float>(biasIntegral)};
    };
    float maximumReferenceCoefficientError = 0.0f;
    float maximumLowRoughnessReferenceRefinementError = 0.0f;
    Uint32 maximumErrorRoughnessRow = 0;
    Uint32 maximumErrorViewColumn = 0;
    size_t maximumErrorCoefficient = 0;
    for (size_t roughnessIndex = 0;
         roughnessIndex < roughnessSamples.size(); ++roughnessIndex) {
        for (size_t viewIndex = 0; viewIndex < viewSamples.size(); ++viewIndex) {
            const auto refinementReference = roughnessIndex == 0
                ? integrateLowRoughnessSlopeQuadratureReference(
                    viewSamples[viewIndex], roughnessSamples[roughnessIndex],
                    48, 256)
                : std::array<float, 2>{};
            const auto reference = roughnessIndex == 0
                ? integrateLowRoughnessSlopeQuadratureReference(
                    viewSamples[viewIndex], roughnessSamples[roughnessIndex],
                    96, 512)
                : integrateUniformHemisphereReference(
                    viewSamples[viewIndex], roughnessSamples[roughnessIndex]);
            if (roughnessIndex == 0) {
                for (size_t coefficient = 0; coefficient < reference.size();
                     ++coefficient) {
                    maximumLowRoughnessReferenceRefinementError = std::max(
                        maximumLowRoughnessReferenceRefinementError,
                        std::fabs(reference[coefficient] -
                                  refinementReference[coefficient]));
                }
            }
            for (size_t coefficient = 0; coefficient < reference.size();
                 ++coefficient) {
                const float referenceError = std::fabs(
                    splitSumValues[roughnessIndex][viewIndex][coefficient] -
                    reference[coefficient]);
                if (referenceError > maximumReferenceCoefficientError) {
                    maximumReferenceCoefficientError = referenceError;
                    maximumErrorRoughnessRow = roughnessSamples[roughnessIndex];
                    maximumErrorViewColumn = viewSamples[viewIndex];
                    maximumErrorCoefficient = coefficient;
                }
                EXPECT_NEAR(splitSumValues[roughnessIndex][viewIndex][coefficient],
                            reference[coefficient], 0.015f)
                    << "Independent GGX-slope Gauss-Legendre BRDF integration: roughness row "
                    << roughnessSamples[roughnessIndex] << ", NdotV column "
                    << viewSamples[viewIndex] << ", coefficient " << coefficient;
            }
        }
    }
    std::fprintf(stderr,
        "PBR_BRDF_REFERENCE uniform-hemisphere 256x512; low-roughness GGX-slope Gauss-Legendre 96x512 max-coefficient-error=%.6f roughnessRow=%u NdotVColumn=%u coefficient=%zu (9 samples)\n",
        maximumReferenceCoefficientError, maximumErrorRoughnessRow,
        maximumErrorViewColumn, maximumErrorCoefficient);
    std::fprintf(stderr,
        "PBR_BRDF_REFERENCE low-roughness GGX-slope Gauss-Legendre 48x256-vs-96x512 refinement-error=%.6f (3 view angles)\n",
        maximumLowRoughnessReferenceRefinementError);
    EXPECT_LE(maximumLowRoughnessReferenceRefinementError, 0.0002f);
    renderer.setEnvironmentRotation(0.0f);
    instance.color[0] = 0.0f;
    instance.color[1] = 0.0f;
    instance.color[2] = 0.0f;
    renderer.updateInstanceData(&instance, 1);
    renderer.setPbrFactors(0.0f, 0.35f, 1.0f, 1.0f);
    renderer.setPrincipledFactors(0.5f, 3.0f, 0.0f, 0.0f, 0.04f);
    const auto rasterHdrPixel = renderCenterPixel();

    auto parityVertexBuffer = makeBuffer(
        positions.data(), sizeof(positions), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto parityIndexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(parityVertexBuffer, nullptr);
    ASSERT_NE(parityIndexBuffer, nullptr);
    constexpr char kHdrParityGeometryId[] = "stinson-hdr-raster-rt-parity";
    addGeometry(kHdrParityGeometryId, parityVertexBuffer.RawPtr(), 3,
                parityIndexBuffer.RawPtr(), 3, float4x4::Identity(), 1, 1,
                positions.data(), indices.data(), normals.data());
    ArtifactCore::RTPbrMaterial parityMaterial;
    parityMaterial.baseColor[0] = 0.0f;
    parityMaterial.baseColor[1] = 0.0f;
    parityMaterial.baseColor[2] = 0.0f;
    parityMaterial.metallic = 0.0f;
    parityMaterial.roughness = 0.35f;
    parityMaterial.specular = 0.5f;
    parityMaterial.ior = 3.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString(kHdrParityGeometryId), parityMaterial));
    manager_->setEnvironmentLighting(
        context_.RawPtr(), renderer.irradianceMapView(),
        renderer.environmentMapView(), renderer.brdfLutView(), true, 1.0f, 0.0f,
        renderer.sheenEnvironmentMapView());
    ArtifactCore::RTSceneLight zeroIntensityLight;
    zeroIntensityLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    manager_->setSceneLights(context_.RawPtr(), &zeroIntensityLight, 1);
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ArtifactCore::RTRayCamera parityCamera;
    parityCamera.origin[0] = 0.0f;
    parityCamera.origin[1] = 0.0f;
    parityCamera.origin[2] = 0.0f;
    parityCamera.target[0] = 0.0f;
    parityCamera.target[1] = 0.0f;
    parityCamera.target[2] = 1.0f;
    parityCamera.verticalFovDegrees = 60.0f;
    ASSERT_TRUE(manager_->traceCamera(context_.RawPtr(), width, height,
                                      parityCamera));

    auto* traceView = manager_->traceOutputSRV();
    ASSERT_NE(traceView, nullptr);
    auto* traceTexture = traceView->GetTexture();
    ASSERT_NE(traceTexture, nullptr);
    TextureDesc traceStagingDesc;
    traceStagingDesc.Name = "StinsonHdrParityReadback";
    traceStagingDesc.Type = RESOURCE_DIM_TEX_2D;
    traceStagingDesc.Width = width;
    traceStagingDesc.Height = height;
    traceStagingDesc.MipLevels = 1;
    traceStagingDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    traceStagingDesc.Usage = USAGE_STAGING;
    traceStagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<ITexture> traceStagingTexture;
    device_->CreateTexture(traceStagingDesc, nullptr, &traceStagingTexture);
    ASSERT_NE(traceStagingTexture, nullptr);
    CopyTextureAttribs traceCopy(
        traceTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        traceStagingTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context_->CopyTexture(traceCopy);
    context_->Flush();
    context_->WaitForIdle();
    MappedTextureSubresource traceMapped{};
    context_->MapTextureSubresource(traceStagingTexture, 0, 0, MAP_READ,
                                    MAP_FLAG_NONE, nullptr, traceMapped);
    ASSERT_NE(traceMapped.pData, nullptr);
    const auto* traceRow = static_cast<const std::uint8_t*>(traceMapped.pData) +
                           (height / 2) * traceMapped.Stride;
    std::array<float, 4> rtHdrPixel{};
    std::memcpy(rtHdrPixel.data(), traceRow + (width / 2) * sizeof(rtHdrPixel),
                sizeof(rtHdrPixel));
    context_->UnmapTextureSubresource(traceStagingTexture, 0, 0);
    const float rasterRadiance = rasterHdrPixel[0] + rasterHdrPixel[1] +
                                 rasterHdrPixel[2];
    const float rtRadiance = rtHdrPixel[0] + rtHdrPixel[1] + rtHdrPixel[2];
    EXPECT_GT(rasterRadiance, 0.01f);
    EXPECT_GT(rtRadiance, 0.01f);
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rasterHdrPixel[channel], rtHdrPixel[channel],
                    std::max(0.015f, std::fabs(rasterHdrPixel[channel]) * 0.12f))
            << "HDR IBL raster/RT channel " << channel;
    }

    renderer.setEnvironmentMaps(renderer.irradianceMapView(),
                                renderer.environmentMapView(),
                                renderer.brdfLutView(), 1.0f,
                                renderer.sheenEnvironmentMapView());
    struct MaterialProbe {
        float metallic;
        float roughness;
        float ior;
        std::array<float, 3> baseColor;
        float sheen = 0.0f;
        float sheenRoughness = 0.5f;
        std::array<float, 3> sheenTint{1.0f, 1.0f, 1.0f};
        float clearcoat = 0.0f;
        float clearcoatRoughness = 0.2f;
        int aoValue = -1;
        std::array<float, 3> clearcoatTint{1.0f, 1.0f, 1.0f};
        std::array<float, 3> specularTint{1.0f, 1.0f, 1.0f};
        float thinFilmThickness = 0.0f;
        float thinFilmIOR = 1.33f;
        float normalStrength = 1.0f;
        float clearcoatNormalStrength = 1.0f;
    };
    constexpr std::array<MaterialProbe, 26> materialProbes{{
        {0.0f, 0.04f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {0.0f, 0.85f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {0.0f, 0.35f, 3.0f, {0.7f, 0.4f, 0.2f}},
        {1.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.8f, 0.15f,
         {1.0f, 0.07f, 0.02f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.8f, 0.85f,
         {1.0f, 0.07f, 0.02f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.8f, 0.15f,
         {0.02f, 0.1f, 1.0f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 1.0f, 0.2f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.8f, 0.15f,
         {1.0f, 0.07f, 0.02f}, 0.0f, 0.2f, 255},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.8f, 0.15f,
         {1.0f, 0.07f, 0.02f}, 0.0f, 0.2f, 64},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 1.0f, 0.05f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 1.0f, 0.85f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 1.0f, 0.2f, -1,
         {0.02f, 0.1f, 1.0f}},
        {0.0f, -0.5f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {0.0f, 1.0f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {-0.5f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {1.5f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 0.0f, 0.2f, -1, {1.0f, 1.0f, 1.0f},
         {0.0f, 1.0f, 1.0f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 0.0f, 0.2f, -1, {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}, 400.0f, 1.33f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 0.0f, 0.2f, -1, {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}, 400.0f, 2.0f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 0.0f, 0.2f, -1, {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}, 0.0f, 1.33f, 0.0f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 0.0f, 0.2f, -1, {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}, 0.0f, 1.33f, 1.0f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 1.0f, 0.2f, -1, {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}, 0.0f, 1.33f, 1.0f, 0.0f},
        {0.0f, 0.35f, 1.5f, {0.7f, 0.4f, 0.2f}, 0.0f, 0.5f,
         {1.0f, 1.0f, 1.0f}, 1.0f, 0.2f, -1, {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}, 0.0f, 1.33f, 1.0f, 1.0f}}};
    std::array<std::array<float, 4>, materialProbes.size()> rasterProbePixels{};
    std::array<std::array<float, 4>, materialProbes.size()> rtProbePixels{};
    QTemporaryDir normalMapDirectory;
    ASSERT_TRUE(normalMapDirectory.isValid());
    const QString parityNormalMapPath = normalMapDirectory.filePath(
        QStringLiteral("parity-normal.png"));
    QImage parityNormalMap(2, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(parityNormalMap.isNull());
    parityNormalMap.fill(QColor(153, 128, 242, 255));
    ASSERT_TRUE(parityNormalMap.save(parityNormalMapPath, "PNG"));
    const QString aoWhitePath = normalMapDirectory.filePath(
        QStringLiteral("parity-ao-white.png"));
    const QString aoDarkPath = normalMapDirectory.filePath(
        QStringLiteral("parity-ao-dark.png"));
    QImage aoWhiteImage(1, 1, QImage::Format_Grayscale8);
    QImage aoDarkImage(1, 1, QImage::Format_Grayscale8);
    ASSERT_FALSE(aoWhiteImage.isNull());
    ASSERT_FALSE(aoDarkImage.isNull());
    aoWhiteImage.fill(255);
    aoDarkImage.fill(64);
    ASSERT_TRUE(aoWhiteImage.save(aoWhitePath, "PNG"));
    ASSERT_TRUE(aoDarkImage.save(aoDarkPath, "PNG"));
    for (size_t probeIndex = 0; probeIndex < materialProbes.size();
         ++probeIndex) {
        const auto& probe = materialProbes[probeIndex];
        instance.color[0] = probe.baseColor[0];
        instance.color[1] = probe.baseColor[1];
        instance.color[2] = probe.baseColor[2];
        renderer.updateInstanceData(&instance, 1);
        renderer.setPbrFactors(probe.metallic, probe.roughness,
                               probe.normalStrength, 1.0f);
        renderer.setSpecularTint(
            QColor::fromRgbF(probe.specularTint[0], probe.specularTint[1],
                             probe.specularTint[2]));
        renderer.setThinFilm(probe.thinFilmThickness, probe.thinFilmIOR);
        renderer.setPrincipledFactors(0.5f, probe.ior, 0.0f,
                                      probe.clearcoat,
                                      probe.clearcoatRoughness, probe.sheen);
        renderer.setSheenFactors(probe.sheenRoughness,
                                 QColor::fromRgbF(probe.sheenTint[0],
                                                  probe.sheenTint[1],
                                                  probe.sheenTint[2]));
        renderer.setClearcoatTint(
            QColor::fromRgbF(probe.clearcoatTint[0],
                             probe.clearcoatTint[1],
                             probe.clearcoatTint[2]),
            probe.clearcoatNormalStrength);
        if (probeIndex == 8 || (probeIndex >= 22 && probeIndex <= 23))
            renderer.setNormalTexture(parityNormalMapPath);
        else if (probeIndex >= 9)
            renderer.setNormalTexture(QString{});
        renderer.setClearcoatNormalTexture(
            probeIndex >= 24 ? parityNormalMapPath : QString{});
        if (probe.aoValue == 255)
            renderer.setOcclusionTexture(aoWhitePath);
        else if (probe.aoValue >= 0)
            renderer.setOcclusionTexture(aoDarkPath);
        else
            renderer.setOcclusionTexture(QString{});
        rasterProbePixels[probeIndex] = renderCenterPixel();

        ArtifactCore::RTPbrMaterial rtMaterial;
        rtMaterial.baseColor[0] = probe.baseColor[0];
        rtMaterial.baseColor[1] = probe.baseColor[1];
        rtMaterial.baseColor[2] = probe.baseColor[2];
        rtMaterial.metallic = probe.metallic;
        rtMaterial.roughness = probe.roughness;
        rtMaterial.normalStrength = probe.normalStrength;
        rtMaterial.specular = 0.5f;
        std::copy(probe.specularTint.begin(), probe.specularTint.end(),
                  rtMaterial.specularTint);
        rtMaterial.thinFilmThickness = probe.thinFilmThickness;
        rtMaterial.thinFilmIOR = probe.thinFilmIOR;
        rtMaterial.ior = probe.ior;
        rtMaterial.sheen = probe.sheen;
        rtMaterial.clearcoat = probe.clearcoat;
        rtMaterial.clearcoatRoughness = probe.clearcoatRoughness;
        std::copy(probe.clearcoatTint.begin(), probe.clearcoatTint.end(),
                  rtMaterial.clearcoatTint);
        rtMaterial.sheenRoughness = probe.sheenRoughness;
        std::copy(probe.sheenTint.begin(), probe.sheenTint.end(),
                  rtMaterial.sheenTint);
        if (probeIndex == 8 || (probeIndex >= 22 && probeIndex <= 23))
            rtMaterial.normalTexture = ArtifactCore::UniString(
                parityNormalMapPath);
        rtMaterial.clearcoatNormalStrength = probe.clearcoatNormalStrength;
        if (probeIndex >= 24)
            rtMaterial.clearcoatNormalTexture = ArtifactCore::UniString(
                parityNormalMapPath);
        if (probe.aoValue >= 0) {
            rtMaterial.occlusionTexture = ArtifactCore::UniString(
                probe.aoValue == 255 ? aoWhitePath : aoDarkPath);
            rtMaterial.occlusionStrength = 1.0f;
        }
        ASSERT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString(kHdrParityGeometryId), rtMaterial));
        ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ASSERT_TRUE(manager_->traceCamera(context_.RawPtr(), width, height,
                                          parityCamera));
        CopyTextureAttribs probeCopy(
            traceTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            traceStagingTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->CopyTexture(probeCopy);
        context_->Flush();
        context_->WaitForIdle();
        MappedTextureSubresource probeMapped{};
        context_->MapTextureSubresource(traceStagingTexture, 0, 0, MAP_READ,
                                        MAP_FLAG_NONE, nullptr, probeMapped);
        ASSERT_NE(probeMapped.pData, nullptr);
        const auto* probeRow =
            static_cast<const std::uint8_t*>(probeMapped.pData) +
            (height / 2) * probeMapped.Stride;
        std::memcpy(rtProbePixels[probeIndex].data(),
                    probeRow + (width / 2) * sizeof(rtProbePixels[probeIndex]),
                    sizeof(rtProbePixels[probeIndex]));
        context_->UnmapTextureSubresource(traceStagingTexture, 0, 0);
        for (size_t channel = 0; channel < 3; ++channel) {
            const float parityTolerance = probeIndex >= 5
                ? 0.05f
                : std::max(0.015f,
                           std::fabs(rasterProbePixels[probeIndex][channel]) *
                               0.12f);
            EXPECT_NEAR(rasterProbePixels[probeIndex][channel],
                        rtProbePixels[probeIndex][channel],
                        parityTolerance)
                << "HDR material probe " << probeIndex << " channel "
                << channel;
        }
    }
    renderer.setOcclusionTexture(QString{});
    const auto probeLuminance = [](const std::array<float, 4>& pixel) {
        return pixel[0] * 0.2126f + pixel[1] * 0.7152f +
               pixel[2] * 0.0722f;
    };
    EXPECT_GT(std::fabs(probeLuminance(rasterProbePixels[0]) -
                        probeLuminance(rasterProbePixels[1])), 0.002f);
    EXPECT_GT(std::fabs(probeLuminance(rasterProbePixels[2]) -
                        probeLuminance(rasterProbePixels[3])), 0.002f);
    float metallicColorDifference = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        metallicColorDifference += std::fabs(
            rasterProbePixels[2][channel] - rasterProbePixels[4][channel]);
    }
    EXPECT_GT(metallicColorDifference, 0.005f);
    EXPECT_GT(std::fabs(probeLuminance(rasterProbePixels[5]) -
                        probeLuminance(rasterProbePixels[6])), 0.002f);
    float sheenTintDifference = 0.0f;
    float rtSheenTintDifference = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        sheenTintDifference += std::fabs(
            rasterProbePixels[5][channel] - rasterProbePixels[7][channel]);
        rtSheenTintDifference += std::fabs(
            rtProbePixels[5][channel] - rtProbePixels[7][channel]);
    }
    EXPECT_GT(sheenTintDifference, 0.005f);
    EXPECT_GT(rtSheenTintDifference, 0.005f)
        << "RT sheen response should vary with sheen tint";
    float rasterThinFilmDifference = 0.0f;
    float rtThinFilmDifference = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterThinFilmDifference += std::fabs(
            rasterProbePixels[20][channel] - rasterProbePixels[19][channel]);
        rtThinFilmDifference += std::fabs(
            rtProbePixels[20][channel] - rtProbePixels[19][channel]);
    }
    EXPECT_GT(rasterThinFilmDifference, 0.001f)
        << "Raster specular response should vary with thin-film thickness";
    EXPECT_GT(rtThinFilmDifference, 0.001f)
        << "RT specular response should vary with thin-film thickness";
    float rasterThinFilmIorDifference = 0.0f;
    float rtThinFilmIorDifference = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterThinFilmIorDifference += std::fabs(
            rasterProbePixels[21][channel] - rasterProbePixels[20][channel]);
        rtThinFilmIorDifference += std::fabs(
            rtProbePixels[21][channel] - rtProbePixels[20][channel]);
    }
    EXPECT_GT(rasterThinFilmIorDifference, 0.001f)
        << "Raster specular response should vary with thin-film IOR";
    EXPECT_GT(rtThinFilmIorDifference, 0.001f)
        << "RT specular response should vary with thin-film IOR";
    float rasterNormalStrengthDifference = 0.0f;
    float rtNormalStrengthDifference = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterNormalStrengthDifference += std::fabs(
            rasterProbePixels[23][channel] - rasterProbePixels[22][channel]);
        rtNormalStrengthDifference += std::fabs(
            rtProbePixels[23][channel] - rtProbePixels[22][channel]);
    }
    EXPECT_GT(rasterNormalStrengthDifference, 0.001f)
        << "Raster normal mapping should respond to normal strength";
    EXPECT_GT(rtNormalStrengthDifference, 0.001f)
        << "RT normal mapping should respond to normal strength";
    float rasterClearcoatNormalStrengthDifference = 0.0f;
    float rtClearcoatNormalStrengthDifference = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterClearcoatNormalStrengthDifference += std::fabs(
            rasterProbePixels[25][channel] - rasterProbePixels[24][channel]);
        rtClearcoatNormalStrengthDifference += std::fabs(
            rtProbePixels[25][channel] - rtProbePixels[24][channel]);
    }
    EXPECT_GT(rasterClearcoatNormalStrengthDifference, 0.001f)
        << "Raster clearcoat normal mapping should respond to its strength";
    EXPECT_GT(rtClearcoatNormalStrengthDifference, 0.001f)
        << "RT clearcoat normal mapping should respond to its strength";
    EXPECT_GT(probeLuminance(rasterProbePixels[9]),
              probeLuminance(rasterProbePixels[10]) + 0.001f)
        << "lower AO should reduce the Raster sheen IBL response";
    EXPECT_GT(probeLuminance(rtProbePixels[9]),
              probeLuminance(rtProbePixels[10]) + 0.001f)
        << "lower AO should reduce the RT sheen IBL response";
    EXPECT_GT(std::fabs(probeLuminance(rasterProbePixels[11]) -
                        probeLuminance(rasterProbePixels[12])),
              0.002f)
        << "Raster clearcoat response should vary with clearcoat roughness";
    EXPECT_GT(std::fabs(probeLuminance(rtProbePixels[11]) -
                        probeLuminance(rtProbePixels[12])),
              0.002f)
        << "RT clearcoat response should vary with clearcoat roughness";
    float rasterClearcoatTintDifference = 0.0f;
    float rtClearcoatTintDifference = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterClearcoatTintDifference += std::fabs(
            rasterProbePixels[8][channel] - rasterProbePixels[13][channel]);
        rtClearcoatTintDifference += std::fabs(
            rtProbePixels[8][channel] - rtProbePixels[13][channel]);
    }
    EXPECT_GT(rasterClearcoatTintDifference, 0.005f)
        << "Raster clearcoat response should vary with clearcoat tint";
    EXPECT_GT(rtClearcoatTintDifference, 0.005f)
        << "RT clearcoat response should vary with clearcoat tint";
    EXPECT_GT(std::fabs(rasterProbePixels[18][0] - rasterProbePixels[19][0]),
              0.001f)
        << "Raster specular response should vary with specular tint";
    EXPECT_GT(std::fabs(rtProbePixels[18][0] - rtProbePixels[19][0]), 0.001f)
        << "RT specular response should vary with specular tint";
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rasterProbePixels[14][channel],
                    rasterProbePixels[0][channel], 1.0e-5f)
            << "negative Raster base roughness should clamp to the 0.04 "
               "shader floor, channel " << channel;
        EXPECT_NEAR(rtProbePixels[14][channel], rtProbePixels[0][channel],
                    1.0e-5f)
            << "negative RT base roughness should clamp to the 0.04 shader "
               "floor, channel " << channel;
    }
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rasterProbePixels[16][channel],
                    rasterProbePixels[2][channel], 1.0e-5f)
            << "negative Raster metallic should clamp to zero, channel "
            << channel;
        EXPECT_NEAR(rtProbePixels[16][channel], rtProbePixels[2][channel],
                    1.0e-5f)
            << "negative RT metallic should clamp to zero, channel "
            << channel;
        EXPECT_NEAR(rasterProbePixels[17][channel],
                    rasterProbePixels[4][channel], 1.0e-5f)
            << "Raster metallic above one should clamp to one, channel "
            << channel;
        EXPECT_NEAR(rtProbePixels[17][channel], rtProbePixels[4][channel],
                    1.0e-5f)
            << "RT metallic above one should clamp to one, channel "
            << channel;
    }

    auto nonUniformTransform = float4x4::Identity();
    nonUniformTransform.m00 = 2.0f;
    nonUniformTransform.m22 = 0.5f;
    instance.transform[0] = 2.0f;
    instance.transform[10] = 0.5f;
    renderer.updateInstanceData(&instance, 1);
    ASSERT_TRUE(manager_->updateInstanceTransform(
        ArtifactCore::UniString(kHdrParityGeometryId), nonUniformTransform));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceCamera(context_.RawPtr(), width, height,
                                      parityCamera));
    CopyTextureAttribs nonUniformCopy(
        traceTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        traceStagingTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context_->CopyTexture(nonUniformCopy);
    context_->Flush();
    context_->WaitForIdle();
    MappedTextureSubresource nonUniformMapped{};
    context_->MapTextureSubresource(traceStagingTexture, 0, 0, MAP_READ,
                                    MAP_FLAG_NONE, nullptr, nonUniformMapped);
    ASSERT_NE(nonUniformMapped.pData, nullptr);
    const auto* nonUniformRow =
        static_cast<const std::uint8_t*>(nonUniformMapped.pData) +
        (height / 2) * nonUniformMapped.Stride;
    std::array<float, 4> rtNonUniformPixel{};
    std::memcpy(rtNonUniformPixel.data(),
                nonUniformRow + (width / 2) * sizeof(rtNonUniformPixel),
                sizeof(rtNonUniformPixel));
    context_->UnmapTextureSubresource(traceStagingTexture, 0, 0);
    std::array<float, 4> rasterNonUniformPixel = renderCenterPixel();
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rasterNonUniformPixel[channel], rtNonUniformPixel[channel],
                    std::max(0.015f,
                             std::fabs(rasterNonUniformPixel[channel]) * 0.12f))
            << "Non-uniform-scale normal raster/RT channel " << channel;
    }

    constexpr std::array<std::array<float, 4>, 6> faceColors{{
        {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}}};
    TextureDesc directionCubeDesc;
    directionCubeDesc.Name = "RayTracingAcceptanceDirectionalCube";
    directionCubeDesc.Type = RESOURCE_DIM_TEX_CUBE;
    directionCubeDesc.Width = 1;
    directionCubeDesc.Height = 1;
    directionCubeDesc.ArraySize = 6;
    directionCubeDesc.MipLevels = 1;
    directionCubeDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    directionCubeDesc.Usage = USAGE_IMMUTABLE;
    directionCubeDesc.BindFlags = BIND_SHADER_RESOURCE;
    std::array<TextureSubResData, 6> directionCubeFaces{};
    for (size_t face = 0; face < directionCubeFaces.size(); ++face) {
        directionCubeFaces[face].pData = faceColors[face].data();
        directionCubeFaces[face].Stride = sizeof(faceColors[face]);
    }
    TextureData directionCubeData;
    directionCubeData.pSubResources = directionCubeFaces.data();
    directionCubeData.NumSubresources =
        static_cast<Uint32>(directionCubeFaces.size());
    RefCntAutoPtr<ITexture> directionCubeTexture;
    device_->CreateTexture(directionCubeDesc, &directionCubeData,
                           &directionCubeTexture);
    ASSERT_NE(directionCubeTexture, nullptr);
    auto directionCubeView = directionCubeTexture->GetDefaultView(
        TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(directionCubeView, nullptr);
    renderer.setEnvironmentMaps(irradianceView, directionCubeView,
                                brdfView, 1.0f);
    const auto rasterDirectionPixel = renderCenterPixel();
    manager_->setEnvironmentLighting(
        context_.RawPtr(), irradianceView, directionCubeView, brdfView,
        true, 1.0f, 0.0f);
    ASSERT_TRUE(manager_->traceCamera(context_.RawPtr(), width, height,
                                      parityCamera));
    CopyTextureAttribs directionCopy(
        traceTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        traceStagingTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context_->CopyTexture(directionCopy);
    context_->Flush();
    context_->WaitForIdle();
    MappedTextureSubresource directionMapped{};
    context_->MapTextureSubresource(traceStagingTexture, 0, 0, MAP_READ,
                                    MAP_FLAG_NONE, nullptr, directionMapped);
    ASSERT_NE(directionMapped.pData, nullptr);
    const auto* directionRow =
        static_cast<const std::uint8_t*>(directionMapped.pData) +
        (height / 2) * directionMapped.Stride;
    std::array<float, 4> rtDirectionPixel{};
    std::memcpy(rtDirectionPixel.data(),
                directionRow + (width / 2) * sizeof(rtDirectionPixel),
                sizeof(rtDirectionPixel));
    context_->UnmapTextureSubresource(traceStagingTexture, 0, 0);
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(rasterDirectionPixel[channel], rtDirectionPixel[channel],
                    0.01f)
            << "Directional cubemap raster/RT channel " << channel;
    }

    ArtifactCore::Light diffuseLight(ArtifactCore::LightType::Directional);
    diffuseLight.setDirection({0.0f, 0.0f, 1.0f});
    diffuseLight.setColor({1.0f, 1.0f, 1.0f});
    diffuseLight.setIntensity(4.0f);
    diffuseLight.setCastsShadows(false);
    renderer.setSceneLights(std::vector<ArtifactCore::Light>{diffuseLight});
    instance.color[0] = instance.color[1] = instance.color[2] = 0.7f;
    renderer.updateInstanceData(&instance, 1);
    renderer.setPbrFactors(0.0f, 1.0f, 1.0f, 1.0f);
    renderer.setPrincipledFactors(0.0f, 1.5f, 0.0f, 0.0f, 0.04f);
    const auto captureRasterDiffuseRoughness = [&](float diffuseRoughness) {
        renderer.setDiffuseRoughness(diffuseRoughness);
        return renderCenterPixel();
    };
    const auto smoothRasterDiffuse = captureRasterDiffuseRoughness(0.0f);
    const auto roughRasterDiffuse = captureRasterDiffuseRoughness(1.0f);
    float rasterDiffuseRoughnessResponse = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterDiffuseRoughnessResponse = std::max(
            rasterDiffuseRoughnessResponse,
            std::fabs(roughRasterDiffuse[channel] - smoothRasterDiffuse[channel]));
    }
    EXPECT_GT(rasterDiffuseRoughnessResponse, 0.001f)
        << "Raster diffuse roughness should change direct diffuse response";
    const auto belowRangeRasterDiffuse = captureRasterDiffuseRoughness(-0.5f);
    const auto aboveRangeRasterDiffuse = captureRasterDiffuseRoughness(1.5f);
    const auto nonFiniteRasterDiffuse = captureRasterDiffuseRoughness(
        std::numeric_limits<float>::quiet_NaN());
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowRangeRasterDiffuse[channel],
                    smoothRasterDiffuse[channel], 1.0e-5f)
            << "Raster diffuse roughness below zero should clamp to zero";
        EXPECT_NEAR(aboveRangeRasterDiffuse[channel],
                    roughRasterDiffuse[channel], 1.0e-5f)
            << "Raster diffuse roughness above one should clamp to one";
        EXPECT_NEAR(nonFiniteRasterDiffuse[channel],
                    smoothRasterDiffuse[channel], 1.0e-5f)
            << "non-finite Raster diffuse roughness should use its zero fallback";
    }

    renderer.setPrincipledFactors(0.5f, 1.5f, 0.0f, 0.0f, 0.04f);
    const auto captureRasterThinFilm = [&](float thicknessNm, float filmIor) {
        renderer.setThinFilm(thicknessNm, filmIor);
        return renderCenterPixel();
    };
    const auto noRasterThinFilm = captureRasterThinFilm(0.0f, 1.33f);
    const auto enabledRasterThinFilm = captureRasterThinFilm(400.0f, 1.33f);
    float rasterThinFilmResponse = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterThinFilmResponse = std::max(
            rasterThinFilmResponse,
            std::fabs(enabledRasterThinFilm[channel] - noRasterThinFilm[channel]));
    }
    EXPECT_GT(rasterThinFilmResponse, 0.001f)
        << "Raster specular response should vary with finite film thickness";
    const auto belowRasterFilmThickness = captureRasterThinFilm(-10.0f, 1.33f);
    const auto maximumRasterFilmThickness = captureRasterThinFilm(1000.0f, 1.33f);
    const auto aboveRasterFilmThickness = captureRasterThinFilm(1200.0f, 1.33f);
    const auto nonFiniteRasterFilmThickness = captureRasterThinFilm(
        std::numeric_limits<float>::quiet_NaN(), 1.33f);
    const auto minimumRasterFilmIor = captureRasterThinFilm(400.0f, 1.0f);
    const auto belowRasterFilmIor = captureRasterThinFilm(400.0f, 0.5f);
    const auto maximumRasterFilmIor = captureRasterThinFilm(400.0f, 2.5f);
    const auto aboveRasterFilmIor = captureRasterThinFilm(400.0f, 3.0f);
    const auto nonFiniteRasterFilmIor = captureRasterThinFilm(
        400.0f, std::numeric_limits<float>::quiet_NaN());
    for (size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowRasterFilmThickness[channel],
                    noRasterThinFilm[channel], 1.0e-5f)
            << "Raster film thickness below zero should clamp to zero";
        EXPECT_NEAR(maximumRasterFilmThickness[channel],
                    aboveRasterFilmThickness[channel], 1.0e-5f)
            << "Raster film thickness above 1000 nm should clamp to 1000 nm";
        EXPECT_NEAR(nonFiniteRasterFilmThickness[channel],
                    noRasterThinFilm[channel], 1.0e-5f)
            << "non-finite Raster film thickness should use its zero fallback";
        EXPECT_NEAR(minimumRasterFilmIor[channel],
                    belowRasterFilmIor[channel], 1.0e-5f)
            << "Raster film IOR below one should clamp to one";
        EXPECT_NEAR(maximumRasterFilmIor[channel],
                    aboveRasterFilmIor[channel], 1.0e-5f)
            << "Raster film IOR above 2.5 should clamp to 2.5";
        EXPECT_NEAR(enabledRasterThinFilm[channel],
                    nonFiniteRasterFilmIor[channel], 1.0e-5f)
            << "non-finite Raster film IOR should use its 1.33 fallback";
    }

    const QVector3D neutralSubsurfaceRadius(1.0f, 1.0f, 1.0f);
    const auto captureRasterSubsurface = [&](float weight, const QColor& color,
                                             const QVector3D& radius,
                                             float ior = 1.4f,
                                             float anisotropy = 0.0f) {
        renderer.setSubsurfaceFactors(weight, color, radius,
                                      ior, anisotropy);
        return renderCenterPixel();
    };
    const QColor whiteSubsurfaceColor(255, 255, 255);
    const auto burleyDiffuse = captureRasterSubsurface(
        0.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius);
    const auto fullRasterSubsurface = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius);
    float rasterSubsurfaceResponse = 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
        rasterSubsurfaceResponse = std::max(
            rasterSubsurfaceResponse,
            std::fabs(fullRasterSubsurface[channel] - burleyDiffuse[channel]));
    }
    EXPECT_GT(rasterSubsurfaceResponse, 0.001f)
        << "Raster subsurface weight should change direct diffuse response";
    for (std::size_t tintChannel = 0; tintChannel < 3; ++tintChannel) {
        std::array<float, 3> tint{1.0f, 1.0f, 1.0f};
        tint[tintChannel] = 0.0f;
        const auto zeroTintChannel = captureRasterSubsurface(
            1.0f, QColor::fromRgbF(tint[0], tint[1], tint[2]),
            neutralSubsurfaceRadius);
        EXPECT_LT(zeroTintChannel[tintChannel],
                  fullRasterSubsurface[tintChannel] - 0.001f)
            << "Raster subsurface tint should reduce its channel "
            << tintChannel;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            if (channel != tintChannel) {
                EXPECT_NEAR(zeroTintChannel[channel],
                            fullRasterSubsurface[channel], 1.0e-5f)
                    << "Raster subsurface tint channel " << tintChannel
                    << " should preserve output channel " << channel;
            }
        }
    }
    for (std::size_t radiusChannel = 0; radiusChannel < 3; ++radiusChannel) {
        std::array<float, 3> radius{1.0f, 1.0f, 1.0f};
        radius[radiusChannel] = 0.0f;
        const auto zeroRadiusChannel = captureRasterSubsurface(
            1.0f, whiteSubsurfaceColor,
            QVector3D(radius[0], radius[1], radius[2]));
        EXPECT_LT(zeroRadiusChannel[radiusChannel],
                  fullRasterSubsurface[radiusChannel] - 0.001f)
            << "Raster subsurface radius should reduce its channel "
            << radiusChannel;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            if (channel != radiusChannel) {
                EXPECT_NEAR(zeroRadiusChannel[channel],
                            fullRasterSubsurface[channel], 1.0e-5f)
                    << "Raster subsurface radius channel " << radiusChannel
                    << " should preserve output channel " << channel;
            }
        }
    }
    const QVector3D maximumSubsurfaceRadius(10.0f, 10.0f, 10.0f);
    const auto maximumRadiusResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, maximumSubsurfaceRadius);
    const QVector3D overMaximumSubsurfaceRadius(20.0f, 20.0f, 20.0f);
    const auto overMaximumRadiusResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, overMaximumSubsurfaceRadius);
    const QVector3D infiniteSubsurfaceRadius(
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity());
    const QVector3D defaultSubsurfaceRadius(1.0f, 0.2f, 0.1f);
    const auto nonFiniteRadiusResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, infiniteSubsurfaceRadius);
    const auto defaultRadiusResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, defaultSubsurfaceRadius);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(overMaximumRadiusResponse[channel],
                    maximumRadiusResponse[channel], 1.0e-5f)
            << "Raster subsurface radius above ten should clamp to ten, channel "
            << channel;
        EXPECT_NEAR(nonFiniteRadiusResponse[channel],
                    defaultRadiusResponse[channel], 1.0e-5f)
            << "non-finite Raster subsurface radius should use its channel "
            << "default, channel " << channel;
    }
    const auto minimumIorResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.0f);
    const auto belowMinimumIorResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 0.5f);
    const auto maximumIorResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 2.0f);
    const auto aboveMaximumIorResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 3.0f);
    const auto nonFiniteIorResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius,
        std::numeric_limits<float>::quiet_NaN());
    const auto defaultIorResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f);
    float minimumIorEffect = 0.0f;
    float maximumIorEffect = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumIorResponse[channel],
                    minimumIorResponse[channel], 1.0e-5f)
            << "Raster subsurface IOR below one should clamp to one, channel "
            << channel;
        EXPECT_NEAR(aboveMaximumIorResponse[channel],
                    maximumIorResponse[channel], 1.0e-5f)
            << "Raster subsurface IOR above two should clamp to two, channel "
            << channel;
        EXPECT_NEAR(nonFiniteIorResponse[channel],
                    defaultIorResponse[channel], 1.0e-5f)
            << "non-finite Raster subsurface IOR should use 1.4, channel "
            << channel;
        minimumIorEffect = std::max(minimumIorEffect,
            std::fabs(minimumIorResponse[channel] - maximumIorResponse[channel]));
        maximumIorEffect = std::max(maximumIorEffect,
            std::fabs(maximumIorResponse[channel] - defaultIorResponse[channel]));
    }
    EXPECT_GT(minimumIorEffect, 0.001f)
        << "Raster subsurface IOR should change direct diffuse response";
    EXPECT_GT(maximumIorEffect, 0.001f)
        << "Raster subsurface IOR should respond through its supported range";
    const auto neutralAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f, 0.0f);
    const auto backwardAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f, -0.8f);
    const auto forwardAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f, 0.8f);
    const auto minimumAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f, -0.9f);
    const auto belowMinimumAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f, -2.0f);
    const auto maximumAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f, 0.9f);
    const auto aboveMaximumAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f, 2.0f);
    const auto nonFiniteAnisotropyResponse = captureRasterSubsurface(
        1.0f, whiteSubsurfaceColor, neutralSubsurfaceRadius, 1.4f,
        std::numeric_limits<float>::quiet_NaN());
    float anisotropyEffect = 0.0f;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(belowMinimumAnisotropyResponse[channel],
                    minimumAnisotropyResponse[channel], 1.0e-5f)
            << "Raster subsurface anisotropy below -0.9 should clamp, channel "
            << channel;
        EXPECT_NEAR(aboveMaximumAnisotropyResponse[channel],
                    maximumAnisotropyResponse[channel], 1.0e-5f)
            << "Raster subsurface anisotropy above 0.9 should clamp, channel "
            << channel;
        EXPECT_NEAR(nonFiniteAnisotropyResponse[channel],
                    neutralAnisotropyResponse[channel], 1.0e-5f)
            << "non-finite Raster subsurface anisotropy should use zero, channel "
            << channel;
        anisotropyEffect = std::max(anisotropyEffect,
            std::fabs(backwardAnisotropyResponse[channel] -
                      forwardAnisotropyResponse[channel]));
    }
    EXPECT_GT(anisotropyEffect, 0.001f)
        << "Raster subsurface anisotropy should change direct scatter response";
}

TEST_F(RayTracingAcceptanceTest, CapturesPbrSphereSweepUnderStinsonHdr) {
    constexpr Uint32 width = 256;
    constexpr Uint32 height = 256;
    constexpr Uint32 sphereStacks = 24;
    constexpr Uint32 sphereSlices = 48;
    constexpr float sphereRadius = 0.44f;
    constexpr float sphereCenterZ = 0.5f;
    constexpr float pi = 3.14159265358979323846f;

    std::vector<float> positions;
    std::vector<float> normals;
    positions.reserve((sphereStacks + 1) * (sphereSlices + 1) * 3);
    normals.reserve((sphereStacks + 1) * (sphereSlices + 1) * 3);
    for (Uint32 stack = 0; stack <= sphereStacks; ++stack) {
        const float theta = pi * static_cast<float>(stack) / sphereStacks;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (Uint32 slice = 0; slice <= sphereSlices; ++slice) {
            const float phi = 2.0f * pi * static_cast<float>(slice) / sphereSlices;
            const float nx = sinTheta * std::cos(phi);
            const float ny = cosTheta;
            const float nz = sinTheta * std::sin(phi);
            positions.insert(positions.end(), {
                sphereRadius * nx, sphereRadius * ny,
                sphereCenterZ + sphereRadius * nz});
            normals.insert(normals.end(), {nx, ny, nz});
        }
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(sphereStacks * sphereSlices * 6);
    for (Uint32 stack = 0; stack < sphereStacks; ++stack) {
        for (Uint32 slice = 0; slice < sphereSlices; ++slice) {
            const auto a = stack * (sphereSlices + 1) + slice;
            const auto b = a + sphereSlices + 1;
            indices.insert(indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }

    const auto projectRoot = std::filesystem::path(__FILE__)
                                 .parent_path().parent_path().parent_path();
    const auto studioHdrPath = projectRoot /
        "libs/openusd/pxr/imaging/hdx/textures/StinsonBeach.hdr";
    ASSERT_TRUE(std::filesystem::exists(studioHdrPath));
    const auto studioHdrPathString =
        QString::fromStdString(studioHdrPath.string());

    ArtifactCore::GpuContext gpuContext(device_.RawPtr(), context_.RawPtr());
    ArtifactCore::MeshRenderer renderer(gpuContext);
    renderer.setRenderTargetFormat(TEX_FORMAT_RGBA32_FLOAT);
    renderer.initialize(1, positions.size() / 3, indices.size());
    renderer.updateMeshGeometry(positions.data(), normals.data(), nullptr,
                                indices.data());
    constexpr float cameraZ = -1.5f;
    constexpr float verticalFovDegrees = 48.0f;
    constexpr float environmentRotationDegrees = 31.0f;
    constexpr float nearPlane = 0.01f;
    constexpr float farPlane = 100.0f;
    const float projectionScale = 1.0f /
        std::tan(verticalFovDegrees * pi / 360.0f);
    const std::array<float, 16> identity{
        1, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 1};
    const std::array<float, 16> viewMatrix{
        1, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 1, 0, 0, 0, -cameraZ, 1};
    const std::array<float, 16> projectionMatrix{
        projectionScale, 0, 0, 0,
        0, projectionScale, 0, 0,
        0, 0, farPlane / (farPlane - nearPlane), 1,
        0, 0, -nearPlane * farPlane / (farPlane - nearPlane), 0};
    renderer.setViewMatrix(viewMatrix.data());
    renderer.setProjectionMatrix(projectionMatrix.data());
    ASSERT_TRUE(renderer.setEnvironmentMap(studioHdrPathString, 1.0f));
    renderer.setEnvironmentRotation(environmentRotationDegrees);
    std::array<float, 4> lastSavedCenterPixel{};

    ArtifactCore::InstanceData instance{};
    std::copy(identity.begin(), identity.end(), instance.transform);
    std::copy(identity.begin(), identity.end(), instance.previousTransform);
    instance.color[0] = 0.72f;
    instance.color[1] = 0.30f;
    instance.color[2] = 0.08f;
    instance.color[3] = 1.0f;
    instance.weight = 1.0f;
    renderer.updateInstanceData(&instance, 1);

    TextureDesc colorDesc;
    colorDesc.Name = "PbrSphereVisualReviewColor";
    colorDesc.Type = RESOURCE_DIM_TEX_2D;
    colorDesc.Width = width;
    colorDesc.Height = height;
    colorDesc.MipLevels = 1;
    colorDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    colorDesc.Usage = USAGE_DEFAULT;
    colorDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RefCntAutoPtr<ITexture> colorTexture;
    device_->CreateTexture(colorDesc, nullptr, &colorTexture);
    ASSERT_NE(colorTexture, nullptr);
    auto colorView = colorTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    ASSERT_NE(colorView, nullptr);

    TextureDesc depthDesc;
    depthDesc.Name = "PbrSphereVisualReviewDepth";
    depthDesc.Type = RESOURCE_DIM_TEX_2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.Format = TEX_FORMAT_D32_FLOAT;
    depthDesc.Usage = USAGE_DEFAULT;
    depthDesc.BindFlags = BIND_DEPTH_STENCIL;
    RefCntAutoPtr<ITexture> depthTexture;
    device_->CreateTexture(depthDesc, nullptr, &depthTexture);
    ASSERT_NE(depthTexture, nullptr);
    auto depthView = depthTexture->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    ASSERT_NE(depthView, nullptr);

    const auto saveLinearFrame = [&](ITexture* source,
                                     const std::string& fileName) {
        TextureDesc stagingDesc;
        stagingDesc.Name = "PbrSphereVisualReviewReadback";
        stagingDesc.Type = RESOURCE_DIM_TEX_2D;
        stagingDesc.Width = width;
        stagingDesc.Height = height;
        stagingDesc.MipLevels = 1;
        stagingDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
        stagingDesc.Usage = USAGE_STAGING;
        stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
        RefCntAutoPtr<ITexture> stagingTexture;
        device_->CreateTexture(stagingDesc, nullptr, &stagingTexture);
        EXPECT_NE(stagingTexture, nullptr);
        if (!stagingTexture) return false;
        CopyTextureAttribs copy(source,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION, stagingTexture,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->CopyTexture(copy);
        context_->Flush();
        context_->WaitForIdle();
        MappedTextureSubresource mapped{};
        context_->MapTextureSubresource(stagingTexture, 0, 0, MAP_READ,
            MAP_FLAG_NONE, nullptr, mapped);
        EXPECT_NE(mapped.pData, nullptr);
        if (!mapped.pData) return false;
        const auto* centerRow = static_cast<const std::uint8_t*>(mapped.pData) +
            (height / 2) * mapped.Stride;
        std::memcpy(lastSavedCenterPixel.data(),
            centerRow + (width / 2) * sizeof(lastSavedCenterPixel),
            sizeof(lastSavedCenterPixel));

        const auto acesToneMap = [](float value) {
            const float x = std::max(value, 0.0f);
            return std::clamp((x * (2.51f * x + 0.03f)) /
                (x * (2.43f * x + 0.59f) + 0.14f), 0.0f, 1.0f);
        };
        QImage image(width, height, QImage::Format_RGBA8888);
        for (Uint32 y = 0; y < height; ++y) {
            const auto* sourceRow =
                static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.Stride;
            auto* destinationRow = image.scanLine(static_cast<int>(y));
            for (Uint32 x = 0; x < width; ++x) {
                const auto* sourcePixel = reinterpret_cast<const float*>(
                    sourceRow + x * sizeof(float) * 4);
                auto* destinationPixel = destinationRow + x * 4;
                for (Uint32 channel = 0; channel < 3; ++channel) {
                    const float mappedValue = acesToneMap(sourcePixel[channel]);
                    const float srgb = mappedValue <= 0.0031308f
                        ? mappedValue * 12.92f
                        : 1.055f * std::pow(mappedValue, 1.0f / 2.4f) - 0.055f;
                    destinationPixel[channel] = static_cast<std::uint8_t>(
                        std::lround(std::clamp(srgb, 0.0f, 1.0f) * 255.0f));
                }
                destinationPixel[3] = 255;
            }
        }
        context_->UnmapTextureSubresource(stagingTexture, 0, 0);
        const char* configuredOutputDir =
            std::getenv("ARTIFACT_RT_FRAME_OUTPUT_DIR");
        const auto outputDir = configuredOutputDir && configuredOutputDir[0] != '\0'
            ? std::filesystem::path(configuredOutputDir)
            : std::filesystem::current_path() / "ray-tracing-frames";
        std::filesystem::create_directories(outputDir);
        const auto outputPath = outputDir / fileName;
        EXPECT_TRUE(image.save(QString::fromStdString(outputPath.string()), "PNG"))
            << "Could not save PBR visual-review frame: " << outputPath.string();
        return true;
    };
    const auto renderRaster = [&](const std::string& fileName) {
        const float clearColor[4]{0.018f, 0.022f, 0.030f, 1.0f};
        context_->SetRenderTargets(1, &colorView, depthView,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->ClearRenderTarget(colorView, clearColor,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->ClearDepthStencil(depthView, CLEAR_DEPTH_FLAG, 1.0f, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const Viewport viewport{0.0f, 0.0f, static_cast<float>(width),
                                static_cast<float>(height), 0.0f, 1.0f};
        context_->SetViewports(1, &viewport, width, height);
        renderer.prepare(context_.RawPtr());
        renderer.draw(context_.RawPtr(), 1);
        context_->SetRenderTargets(0, nullptr, nullptr,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        return saveLinearFrame(colorTexture, fileName);
    };

    auto vertexBuffer = makeBuffer(positions.data(),
        positions.size() * sizeof(float), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(indices.data(),
        indices.size() * sizeof(std::uint32_t), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("pbr-visual-review-sphere", vertexBuffer.RawPtr(),
        static_cast<Uint32>(positions.size() / 3), indexBuffer.RawPtr(),
        static_cast<Uint32>(indices.size()), float4x4::Identity(), 1, 1,
        positions.data(), indices.data(), normals.data());
    ArtifactCore::RTRayCamera camera;
    camera.origin[0] = 0.0f;
    camera.origin[1] = 0.0f;
    camera.origin[2] = cameraZ;
    camera.target[0] = 0.0f;
    camera.target[1] = 0.0f;
    camera.target[2] = sphereCenterZ;
    camera.verticalFovDegrees = verticalFovDegrees;
    ArtifactCore::RTSceneLight zeroIntensityLight;
    zeroIntensityLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Directional);
    manager_->setSceneLights(context_.RawPtr(), &zeroIntensityLight, 1);
    manager_->setEnvironmentLighting(context_.RawPtr(),
        renderer.irradianceMapView(), renderer.environmentMapView(),
        renderer.brdfLutView(), true, 1.0f,
        environmentRotationDegrees,
        renderer.sheenEnvironmentMapView());

    struct VisualMaterial {
        float metallic;
        float roughness;
        std::array<float, 3> color;
        const char* label;
    };
    constexpr std::array<VisualMaterial, 5> materials{{
        {0.0f, 0.04f, {0.72f, 0.30f, 0.08f}, "dielectric-r004"},
        {0.0f, 0.20f, {0.72f, 0.30f, 0.08f}, "dielectric-r020"},
        {0.0f, 0.50f, {0.72f, 0.30f, 0.08f}, "dielectric-r050"},
        {0.0f, 0.85f, {0.72f, 0.30f, 0.08f}, "dielectric-r085"},
        {1.0f, 0.24f, {0.82f, 0.48f, 0.12f}, "metal-r024"}}};
    std::vector<std::array<float, 4>> unrotatedBackgroundReference;
    for (const auto& material : materials) {
        instance.color[0] = material.color[0];
        instance.color[1] = material.color[1];
        instance.color[2] = material.color[2];
        renderer.updateInstanceData(&instance, 1);
        renderer.setPbrFactors(material.metallic, material.roughness, 1.0f, 1.0f);
        renderer.setPrincipledFactors(0.5f, 1.5f, 0.0f, 0.0f, 0.04f);
        ASSERT_TRUE(renderRaster(
            std::string("pbr-stinson-sphere-") + material.label + "-raster.png"));
        const auto rasterCenterPixel = lastSavedCenterPixel;

        ArtifactCore::RTPbrMaterial rtMaterial;
        rtMaterial.baseColor[0] = material.color[0];
        rtMaterial.baseColor[1] = material.color[1];
        rtMaterial.baseColor[2] = material.color[2];
        rtMaterial.metallic = material.metallic;
        rtMaterial.roughness = material.roughness;
        ASSERT_TRUE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("pbr-visual-review-sphere"), rtMaterial));
        ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
        ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
        ASSERT_TRUE(manager_->traceCamera(context_.RawPtr(), width, height, camera));
        auto* rtOutputView = manager_->traceOutputSRV();
        ASSERT_NE(rtOutputView, nullptr);
        if (&material == &materials.back()) {
            unrotatedBackgroundReference = readOutputPixels();
            ASSERT_EQ(unrotatedBackgroundReference.size(),
                      static_cast<size_t>(width) * height);
        }
        ASSERT_TRUE(saveLinearFrame(
            rtOutputView->GetTexture(),
            std::string("pbr-stinson-sphere-") + material.label + "-rt.png"));
        float maximumCenterChannelError = 0.0f;
        for (size_t channel = 0; channel < 3; ++channel) {
            maximumCenterChannelError = std::max(maximumCenterChannelError,
                std::fabs(rasterCenterPixel[channel] -
                          lastSavedCenterPixel[channel]));
        }
        std::fprintf(stderr, "PBR_SPHERE_PARITY %s center-max-abs=%.6f\n",
                     material.label, maximumCenterChannelError);
        EXPECT_LE(maximumCenterChannelError, 0.05f)
            << "Stinson HDR sphere center Raster/RT material=" << material.label;
    }

    // The environment yaw also rotates the world seen by primary rays that
    // miss the scene. Check its live update away from the sphere, where the
    // baseline material center-parity assertion does not apply.
    constexpr float rotatedWorldDegrees = 0.0f;
    manager_->setEnvironmentLighting(context_.RawPtr(),
        renderer.irradianceMapView(), renderer.environmentMapView(),
        renderer.brdfLutView(), true, 1.0f, rotatedWorldDegrees,
        renderer.sheenEnvironmentMapView());
    ASSERT_TRUE(manager_->traceCamera(context_.RawPtr(), width, height, camera));
    const auto rotatedBackground = readOutputPixels();
    ASSERT_EQ(rotatedBackground.size(), unrotatedBackgroundReference.size());
    float maximumWorldBackgroundRotationDelta = 0.0f;
    for (Uint32 y = 0; y < height; ++y) {
        for (Uint32 x = 0; x < width; ++x) {
            if (x >= 32 && x < width - 32 && y >= 32 && y < height - 32) {
                continue;
            }
            const auto index = static_cast<size_t>(y) * width + x;
            for (size_t channel = 0; channel < 3; ++channel) {
                maximumWorldBackgroundRotationDelta = std::max(
                    maximumWorldBackgroundRotationDelta,
                    std::fabs(rotatedBackground[index][channel] -
                              unrotatedBackgroundReference[index][channel]));
            }
        }
    }
    EXPECT_GT(maximumWorldBackgroundRotationDelta, 0.02f)
        << "The HDR world background should respond to environment rotation";
}

TEST_F(RayTracingAcceptanceTest, TracesPlaneAndThreeDimensionalMeshGeometry) {
    // A receiver on the right, wound so its visible side faces the key light.
    constexpr std::array<float, 12> planeVertices{0.05f,  -0.75f, 0.0f, 0.95f, -0.75f, 0.0f,
                                                  0.95f,  0.75f,  0.0f, 0.05f,  0.75f,  0.0f};
    constexpr std::array<std::uint32_t, 6> planeIndices{0, 1, 2, 0, 2, 3};
    // A closed box mesh on the left, positioned to cast a visible shadow.
    constexpr std::array<float, 24> boxVertices{
        -0.45f, -0.65f, -0.30f, 0.45f, -0.65f, -0.30f, 0.45f, 0.65f, -0.30f, -0.45f, 0.65f, -0.30f,
        -0.45f, -0.65f, 0.30f,  0.45f, -0.65f, 0.30f,  0.45f, 0.65f, 0.30f,  -0.45f, 0.65f, 0.30f};
    constexpr std::array<std::uint32_t, 36> boxIndices{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                                       0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
                                                       2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    constexpr float kNormalY = 0.6f;
    constexpr float kNormalZ = 0.8f;
    constexpr std::array<float, 24> boxNormals{
        0.0f, -kNormalY, -kNormalZ, 0.0f, -kNormalY, -kNormalZ,
        0.0f,  kNormalY, -kNormalZ, 0.0f,  kNormalY, -kNormalZ,
        0.0f, -kNormalY,  kNormalZ, 0.0f, -kNormalY,  kNormalZ,
        0.0f,  kNormalY,  kNormalZ, 0.0f,  kNormalY,  kNormalZ};
    auto planeVertexBuffer =
        makeBuffer(planeVertices.data(), sizeof(planeVertices), 3u * sizeof(float),
                   BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto planeIndexBuffer = makeBuffer(planeIndices.data(), sizeof(planeIndices),
        sizeof(std::uint32_t), BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto boxVertexBuffer =
        makeBuffer(boxVertices.data(), sizeof(boxVertices), 3u * sizeof(float),
                   BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto boxIndexBuffer = makeBuffer(boxIndices.data(), sizeof(boxIndices),
        sizeof(std::uint32_t), BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(planeVertexBuffer, nullptr);
    ASSERT_NE(planeIndexBuffer, nullptr);
    ASSERT_NE(boxVertexBuffer, nullptr);
    ASSERT_NE(boxIndexBuffer, nullptr);
    addGeometry("plane-layer", planeVertexBuffer.RawPtr(), 4, planeIndexBuffer.RawPtr(), 6,
                float4x4::Identity(), 1, 1, planeVertices.data(), planeIndices.data());
    ArtifactCore::RTPbrMaterial planePbrMaterial;
    planePbrMaterial.baseColor[0] = 0.60f;
    planePbrMaterial.baseColor[1] = 0.20f;
    planePbrMaterial.baseColor[2] = 0.02f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(ArtifactCore::UniString("plane-layer"),
                                                 planePbrMaterial));
    auto boxTransform = float4x4::Identity();
    boxTransform.m30 = -0.5f;
    boxTransform.m32 = -0.3f;
    addGeometry("box-object", boxVertexBuffer.RawPtr(), 8, boxIndexBuffer.RawPtr(), 36,
                boxTransform, 1, 1, boxVertices.data(), boxIndices.data(), boxNormals.data());
    ArtifactCore::RTPbrMaterial boxPbrMaterial;
    boxPbrMaterial.baseColor[0] = 0.95f;
    boxPbrMaterial.baseColor[1] = 0.55f;
    boxPbrMaterial.baseColor[2] = 0.10f;
    boxPbrMaterial.metallic = 0.75f;
    boxPbrMaterial.roughness = 0.28f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(ArtifactCore::UniString("box-object"),
                                                 boxPbrMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(
        manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize));
    context_->Flush();
    context_->WaitForIdle();
    ASSERT_TRUE(saveOutputFrame("01-plane-and-box.png"));
    expectMiss(31, 31);
    // Box, gap, and receiver. The near receiver sample is shadowed by the box.
    const auto boxPixel = expectHit(8);
    const auto shadowedPlanePixel = expectHit(18);
    const auto litPlanePixel = expectHit(24);
    EXPECT_GT(std::fabs(litPlanePixel[0] - shadowedPlanePixel[0]), 0.005f);
    EXPECT_GT(std::fabs(litPlanePixel[0] - boxPixel[0]), 0.03f);
    ArtifactCore::RTSceneLight greenAmbientLight;
    greenAmbientLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Ambient);
    greenAmbientLight.directionIntensity[3] = 5.0f;
    greenAmbientLight.colorAttenuationConstant[1] = 1.0f;
    greenAmbientLight.colorAttenuationConstant[3] = 1.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenAmbientLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("09-scene-light.png"));
    const auto sceneLitPlanePixel = pixelAt(24, 16);
    EXPECT_GT(sceneLitPlanePixel[1], litPlanePixel[1] + 0.05f);
    ArtifactCore::RTSceneLight greenPointLight;
    greenPointLight.positionType[2] = -1.0f;
    greenPointLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Point);
    greenPointLight.directionIntensity[3] = 5.0f;
    greenPointLight.colorAttenuationConstant[1] = 1.0f;
    greenPointLight.colorAttenuationConstant[3] = 1.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenPointLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("10-scene-light-shadows.png"));
    const auto pointLitPlanePixel = pixelAt(24, 16);
    EXPECT_GT(std::fabs(pointLitPlanePixel[1] - litPlanePixel[1]), 0.01f);
    const auto pointShadowedPlanePixel = pixelAt(18, 16);
    EXPECT_GT(std::fabs(pointLitPlanePixel[1] - pointShadowedPlanePixel[1]), 0.01f);
    ArtifactCore::RTSceneLight greenSpotLight = greenPointLight;
    greenSpotLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Spot);
    greenSpotLight.directionIntensity[2] = 1.0f;
    greenSpotLight.attenuationSpot[2] = 1.0f;
    greenSpotLight.attenuationSpot[3] = 0.90f;
    greenSpotLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenSpotLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto outsideSpotConePixel = pixelAt(24, 16);
    EXPECT_LT(outsideSpotConePixel[1], pointLitPlanePixel[1]);
    manager_->setSceneLights(context_.RawPtr(), &greenPointLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto hardShadowPixels = readOutputPixels();
    greenPointLight.shadowParams[0] = 1.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenPointLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("12-medium-scene-shadow.png"));
    EXPECT_GT(maximumGreenDifference(hardShadowPixels.data()), 0.001f);
    const auto mediumShadowPixels = readOutputPixels();
    greenPointLight.shadowParams[0] = 2.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenPointLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("13-soft-scene-shadow.png"));
    EXPECT_GT(maximumGreenDifference(mediumShadowPixels.data()), 0.001f);
    const auto softPointPixels = readOutputPixels();
    ArtifactCore::RTSceneLight greenAreaLight = greenPointLight;
    greenAreaLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Area);
    greenAreaLight.directionIntensity[2] = 1.0f;
    greenAreaLight.shadowParams[1] = 1.5f;
    greenAreaLight.shadowParams[2] = 1.0f;
    greenAreaLight.shadowParams[3] = 0.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenAreaLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("14-area-light-shadow.png"));
    EXPECT_GT(maximumGreenDifference(softPointPixels.data()), 0.001f);
    greenPointLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenPointLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto noShadowPointPixels = readOutputPixels();
    greenAreaLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &greenAreaLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("15-area-direct-light.png"));
    EXPECT_GT(maximumGreenDifference(noShadowPointPixels.data()), 0.001f);

    manager_->setSceneLights(context_.RawPtr(), nullptr, 0);
    const auto upperBoxPixel = pixelAt(8, 4);
    const auto lowerBoxPixel = pixelAt(8, 27);
    EXPECT_GT(std::fabs(upperBoxPixel[0] - lowerBoxPixel[0]), 0.005f);
    const auto neutralEnvironmentBoxPixel = pixelAt(8, 16);
    const float environmentRadiance[4] = {4.0f, 0.05f, 0.05f, 1.0f};
    TextureDesc environmentDesc;
    environmentDesc.Name = "RayTracingAcceptanceEnvironment";
    environmentDesc.Type = RESOURCE_DIM_TEX_CUBE;
    environmentDesc.Width = 1;
    environmentDesc.Height = 1;
    environmentDesc.ArraySize = 6;
    environmentDesc.MipLevels = 1;
    environmentDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    environmentDesc.Usage = USAGE_IMMUTABLE;
    environmentDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData environmentFaces[6] = {};
    for (auto& face : environmentFaces) {
        face.pData = environmentRadiance;
        face.Stride = sizeof(environmentRadiance);
    }
    TextureData environmentData;
    environmentData.pSubResources = environmentFaces;
    environmentData.NumSubresources = 6;
    RefCntAutoPtr<ITexture> environmentTexture;
    device_->CreateTexture(environmentDesc, &environmentData, &environmentTexture);
    ASSERT_NE(environmentTexture, nullptr);
    auto environmentView = environmentTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(environmentView, nullptr);
    const float diffuseRadiance[4] = {0.05f, 4.0f, 0.05f, 1.0f};
    TextureSubResData diffuseFaces[6] = {};
    for (auto& face : diffuseFaces) {
        face.pData = diffuseRadiance;
        face.Stride = sizeof(diffuseRadiance);
    }
    TextureData diffuseData;
    diffuseData.pSubResources = diffuseFaces;
    diffuseData.NumSubresources = 6;
    RefCntAutoPtr<ITexture> diffuseEnvironmentTexture;
    device_->CreateTexture(environmentDesc, &diffuseData,
                           &diffuseEnvironmentTexture);
    ASSERT_NE(diffuseEnvironmentTexture, nullptr);
    auto diffuseEnvironmentView = diffuseEnvironmentTexture->GetDefaultView(
        TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(diffuseEnvironmentView, nullptr);
    const float brdfValues[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    TextureDesc brdfDesc;
    brdfDesc.Name = "RayTracingAcceptanceBRDFLUT";
    brdfDesc.Type = RESOURCE_DIM_TEX_2D;
    brdfDesc.Width = 1;
    brdfDesc.Height = 1;
    brdfDesc.MipLevels = 1;
    brdfDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    brdfDesc.Usage = USAGE_IMMUTABLE;
    brdfDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData brdfSubresource{};
    brdfSubresource.pData = brdfValues;
    brdfSubresource.Stride = sizeof(brdfValues);
    TextureData brdfData;
    brdfData.pSubResources = &brdfSubresource;
    brdfData.NumSubresources = 1;
    RefCntAutoPtr<ITexture> brdfTexture;
    device_->CreateTexture(brdfDesc, &brdfData, &brdfTexture);
    ASSERT_NE(brdfTexture, nullptr);
    auto brdfView = brdfTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(brdfView, nullptr);
    manager_->setEnvironmentLighting(context_.RawPtr(), environmentView,
                                     environmentView, brdfView, true, 1.0f, 0.0f);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("08-hdri-reflection.png"));
    const auto imageEnvironmentBoxPixel = pixelAt(8, 16);
    const auto imageEnvironmentPlanePixel = pixelAt(24, 16);
    EXPECT_GT(imageEnvironmentBoxPixel[0], neutralEnvironmentBoxPixel[0] + 0.05f);
    manager_->setEnvironmentLighting(context_.RawPtr(), diffuseEnvironmentView,
                                     environmentView, brdfView, true, 1.0f, 0.0f);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("16-dedicated-irradiance.png"));
    const auto dedicatedIrradiancePixel = pixelAt(24, 16);
    EXPECT_GT(dedicatedIrradiancePixel[1], imageEnvironmentPlanePixel[1] + 0.05f);

    const float zeroBrdfValues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    brdfSubresource.pData = zeroBrdfValues;
    brdfSubresource.Stride = sizeof(zeroBrdfValues);
    RefCntAutoPtr<ITexture> zeroBrdfTexture;
    device_->CreateTexture(brdfDesc, &brdfData, &zeroBrdfTexture);
    ASSERT_NE(zeroBrdfTexture, nullptr);
    auto zeroBrdfView = zeroBrdfTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(zeroBrdfView, nullptr);
    manager_->setEnvironmentLighting(context_.RawPtr(), environmentView,
                                     environmentView, zeroBrdfView, true, 1.0f, 0.0f);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("17-brdf-lut-response.png"));
    const auto zeroBrdfBoxPixel = pixelAt(8, 16);
    EXPECT_LT(zeroBrdfBoxPixel[0], imageEnvironmentBoxPixel[0] - 0.05f);
    manager_->setEnvironmentLighting(context_.RawPtr(), nullptr, nullptr,
                                     nullptr, false, 1.0f, 0.0f);
    ArtifactCore::RTPbrMaterial glassPbrMaterial;
    glassPbrMaterial.baseColor[0] = 1.0f;
    glassPbrMaterial.baseColor[1] = 1.0f;
    glassPbrMaterial.baseColor[2] = 1.0f;
    glassPbrMaterial.roughness = 0.10f;
    glassPbrMaterial.ior = 1.5f;
    glassPbrMaterial.transmission = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(ArtifactCore::UniString("box-object"),
                                                  glassPbrMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto neutralTransmissionPixel = pixelAt(8, 16);
    manager_->setEnvironmentLighting(context_.RawPtr(), environmentView,
                                     environmentView, brdfView, true, 1.0f, 0.0f);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("11-transmission-environment.png"));
    const auto environmentTransmissionPixel = pixelAt(8, 16);
    EXPECT_GT(environmentTransmissionPixel[0], neutralTransmissionPixel[0] + 0.10f);
    glassPbrMaterial.transmission = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("box-object"), glassPbrMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto zeroTransmissionEnvironmentPixel = pixelAt(8, 16);
    const float environmentTransmissionResponse = std::max(
        std::fabs(environmentTransmissionPixel[0] -
                  zeroTransmissionEnvironmentPixel[0]),
        std::max(std::fabs(environmentTransmissionPixel[1] -
                           zeroTransmissionEnvironmentPixel[1]),
                 std::fabs(environmentTransmissionPixel[2] -
                           zeroTransmissionEnvironmentPixel[2])));
    EXPECT_GT(environmentTransmissionResponse, 0.01f)
        << "environment transmission should respond to the material amount";
    manager_->setEnvironmentLighting(context_.RawPtr(), nullptr, nullptr,
                                     nullptr, false, 1.0f, 0.0f);
    ASSERT_TRUE(manager_->setInstancePbrMaterial(ArtifactCore::UniString("box-object"),
                                                  boxPbrMaterial));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ArtifactCore::RTRayCamera invalidCamera;
    invalidCamera.up[0] = 0.0f;
    invalidCamera.up[1] = 0.0f;
    invalidCamera.up[2] = 1.0f;
    EXPECT_FALSE(manager_->traceCamera(context_.RawPtr(), kRayTraceTestImageSize,
                                       kRayTraceTestImageSize, invalidCamera));
    const auto dispatchCountBeforeInvalidCameras =
        manager_->capabilities().traceDispatchCount;
    invalidCamera = ArtifactCore::RTRayCamera{};
    invalidCamera.target[0] = invalidCamera.origin[0];
    invalidCamera.target[1] = invalidCamera.origin[1];
    invalidCamera.target[2] = invalidCamera.origin[2];
    EXPECT_FALSE(manager_->traceCamera(context_.RawPtr(), kRayTraceTestImageSize,
                                       kRayTraceTestImageSize, invalidCamera));
    invalidCamera = ArtifactCore::RTRayCamera{};
    invalidCamera.origin[0] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(manager_->traceCamera(context_.RawPtr(), kRayTraceTestImageSize,
                                       kRayTraceTestImageSize, invalidCamera));
    invalidCamera = ArtifactCore::RTRayCamera{};
    invalidCamera.verticalFovDegrees =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(manager_->traceCamera(context_.RawPtr(), kRayTraceTestImageSize,
                                       kRayTraceTestImageSize, invalidCamera));
    EXPECT_EQ(manager_->capabilities().traceDispatchCount,
              dispatchCountBeforeInvalidCameras)
        << "invalid camera inputs should be rejected before ray dispatch";
    ArtifactCore::RTRayCamera offsetCamera;
    offsetCamera.origin[0] = 4.0f;
    offsetCamera.target[0] = 4.0f;
    ASSERT_TRUE(manager_->traceCamera(context_.RawPtr(), kRayTraceTestImageSize,
                                      kRayTraceTestImageSize, offsetCamera));
    expectMiss(8, 18);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    // 3D box object.
    auto scaledBoxTransform = float4x4::Identity();
    scaledBoxTransform.m00 = 0.5f;
    scaledBoxTransform.m11 = 0.5f;
    scaledBoxTransform.m22 = 0.5f;
    scaledBoxTransform.m30 = -0.5f;
    scaledBoxTransform.m32 = -0.3f;
    ASSERT_TRUE(manager_->updateInstanceTransform(ArtifactCore::UniString("box-object"),
                                                  scaledBoxTransform));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(
        manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("02-scaled-box.png"));
    expectHit(8, 16);
    expectMiss(8, 22);
    // The instance scale changes the hit bounds.
    expectMiss(14);
    // The scaled object no longer reaches this column.
    const ArtifactCore::UniString boxId("box-object");
    ASSERT_TRUE(manager_->setInstanceActive(boxId, false));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(
        manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("03-box-disabled.png"));
    expectMiss(8);
    EXPECT_EQ(manager_->capabilities().activeInstanceCount, 1u);
    ASSERT_TRUE(manager_->setInstanceActive(boxId, true));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(
        manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("04-box-enabled.png"));
    expectHit(8, 16);
    constexpr std::array<float, 12> movedPlaneVertices{-0.45f, -0.75f, 0.0f, 0.45f,  -0.75f, 0.0f,
                                                       0.45f,  0.75f,  0.0f, -0.45f, 0.75f,  0.0f};
    ASSERT_TRUE(updateBuffer(planeVertexBuffer.RawPtr(), movedPlaneVertices.data(),
                             sizeof(movedPlaneVertices)));
    ArtifactCore::RTGeometryData updatedPlane;
    updatedPlane.pVertexBuffer = planeVertexBuffer.RawPtr();
    updatedPlane.vertexCount = 4;
    updatedPlane.pIndexBuffer = planeIndexBuffer.RawPtr();
    updatedPlane.indexCount = 6;
    updatedPlane.geometryRevision = 2;
    updatedPlane.topologyRevision = 1;
    ASSERT_TRUE(manager_->createOrUpdateBLAS(ArtifactCore::UniString("plane-layer"), updatedPlane));
    ASSERT_TRUE(manager_->updateShadingGeometry(ArtifactCore::UniString("plane-layer"),
                                                movedPlaneVertices.data(), 4,
                                                planeIndices.data(), 6));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(
        manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("05-plane-moved.png"));
    expectMiss(31, 31);
    expectHit(16);
    EXPECT_EQ(manager_->capabilities().blasUpdateCount, 1u);
    // Restrict the plane to one triangle so the rebuild changes output.
    constexpr std::array<std::uint32_t, 3> changedPlaneIndices{0, 1, 2};
    ASSERT_TRUE(updateBuffer(planeIndexBuffer.RawPtr(), changedPlaneIndices.data(),
                             sizeof(changedPlaneIndices)));
    updatedPlane.indexCount = static_cast<Uint32>(changedPlaneIndices.size());
    updatedPlane.geometryRevision = 3;
    updatedPlane.topologyRevision = 2;
    ASSERT_TRUE(manager_->createOrUpdateBLAS(ArtifactCore::UniString("plane-layer"), updatedPlane));
    ASSERT_TRUE(manager_->updateShadingGeometry(ArtifactCore::UniString("plane-layer"),
                                                movedPlaneVertices.data(), 4,
                                                changedPlaneIndices.data(), 3));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(
        manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("06-plane-topology-rebuilt.png"));
    expectHit(18);
    expectMiss(31, 31);
    EXPECT_EQ(manager_->capabilities().blasRebuildCount, 3u);
    const auto& capabilities = manager_->capabilities();
    EXPECT_EQ(capabilities.registeredBLASCount, 2u);
    EXPECT_EQ(capabilities.activeInstanceCount, 2u);
    EXPECT_EQ(capabilities.traceDispatchCount, 22u);
    EXPECT_GE(capabilities.tlasUpdateCount, 5u);
    ArtifactCore::RTPbrMaterial defaultMaterial;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("box-object"), defaultMaterial));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto defaultMaterialImage = readOutputPixels();
    ArtifactCore::RTPbrMaterial nonFiniteMaterial;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    nonFiniteMaterial.baseColor[0] = nan;
    nonFiniteMaterial.metallic = infinity;
    nonFiniteMaterial.roughness = nan;
    nonFiniteMaterial.ior = nan;
    nonFiniteMaterial.emissionStrength = infinity;
    nonFiniteMaterial.normalUvRotationDegrees = infinity;
    nonFiniteMaterial.normalUvOffsetU = nan;
    nonFiniteMaterial.metallicTextureUvScaleU = infinity;
    nonFiniteMaterial.roughnessTextureUvOffsetV = nan;
    nonFiniteMaterial.clearcoatNormalUvScale = nan;
    nonFiniteMaterial.sheenColorTextureUvScaleU = infinity;
    nonFiniteMaterial.anisotropyTextureUvOffsetU = nan;
    EXPECT_FALSE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("box-object"), nonFiniteMaterial));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto imageAfterRejectedMaterial = readOutputPixels();
    ASSERT_EQ(imageAfterRejectedMaterial.size(), defaultMaterialImage.size());
    float maximumRejectedUpdateDifference = 0.0f;
    for (size_t pixel = 0; pixel < defaultMaterialImage.size(); ++pixel) {
        for (size_t channel = 0; channel < defaultMaterialImage[pixel].size();
             ++channel) {
            maximumRejectedUpdateDifference = std::max(
                maximumRejectedUpdateDifference,
                std::fabs(imageAfterRejectedMaterial[pixel][channel] -
                          defaultMaterialImage[pixel][channel]));
        }
    }
    EXPECT_LE(maximumRejectedUpdateDifference, 1.0e-5f);

    using Material = ArtifactCore::RTPbrMaterial;
    const std::array nonFiniteScalars{
        std::pair{"baseColorAlpha", &Material::baseColorAlpha},
        std::pair{"diffuseRoughness", &Material::diffuseRoughness},
        std::pair{"specular", &Material::specular},
        std::pair{"thinFilmThickness", &Material::thinFilmThickness},
        std::pair{"thinFilmIOR", &Material::thinFilmIOR},
        std::pair{"transmission", &Material::transmission},
        std::pair{"clearcoat", &Material::clearcoat},
        std::pair{"clearcoatRoughness", &Material::clearcoatRoughness},
        std::pair{"clearcoatIOR", &Material::clearcoatIOR},
        std::pair{"clearcoatNormalStrength", &Material::clearcoatNormalStrength},
        std::pair{"sheen", &Material::sheen},
        std::pair{"sheenRoughness", &Material::sheenRoughness},
        std::pair{"subsurface", &Material::subsurface},
        std::pair{"subsurfaceIOR", &Material::subsurfaceIOR},
        std::pair{"subsurfaceAnisotropy", &Material::subsurfaceAnisotropy},
        std::pair{"anisotropy", &Material::anisotropy},
        std::pair{"anisotropyRotationDegrees", &Material::anisotropyRotationDegrees},
        std::pair{"emissionStrength", &Material::emissionStrength},
        std::pair{"opacity", &Material::opacity},
        std::pair{"alphaCutoff", &Material::alphaCutoff},
        std::pair{"thinWall", &Material::thinWall},
        std::pair{"normalStrength", &Material::normalStrength},
        std::pair{"occlusionStrength", &Material::occlusionStrength},
        std::pair{"baseColorUvScale", &Material::baseColorUvScale},
        std::pair{"baseColorUvRotationDegrees", &Material::baseColorUvRotationDegrees},
        std::pair{"baseColorUvOffsetU", &Material::baseColorUvOffsetU},
        std::pair{"baseColorUvOffsetV", &Material::baseColorUvOffsetV},
        std::pair{"metallicRoughnessUvScale", &Material::metallicRoughnessUvScale},
        std::pair{"metallicRoughnessUvOffsetU", &Material::metallicRoughnessUvOffsetU},
        std::pair{"metallicRoughnessUvOffsetV", &Material::metallicRoughnessUvOffsetV},
        std::pair{"metallicRoughnessUvRotationDegrees",
                  &Material::metallicRoughnessUvRotationDegrees},
        std::pair{"clearcoatTextureUvScale", &Material::clearcoatTextureUvScale},
        std::pair{"clearcoatTextureUvOffsetU", &Material::clearcoatTextureUvOffsetU},
        std::pair{"clearcoatTextureUvOffsetV", &Material::clearcoatTextureUvOffsetV},
        std::pair{"clearcoatTextureUvRotationDegrees",
                  &Material::clearcoatTextureUvRotationDegrees},
        std::pair{"clearcoatRoughnessTextureUvScale",
                  &Material::clearcoatRoughnessTextureUvScale},
        std::pair{"clearcoatRoughnessTextureUvOffsetU",
                  &Material::clearcoatRoughnessTextureUvOffsetU},
        std::pair{"clearcoatRoughnessTextureUvOffsetV",
                  &Material::clearcoatRoughnessTextureUvOffsetV},
        std::pair{"clearcoatRoughnessTextureUvRotationDegrees",
                  &Material::clearcoatRoughnessTextureUvRotationDegrees},
        std::pair{"clearcoatNormalUvRotationDegrees",
                  &Material::clearcoatNormalUvRotationDegrees},
        std::pair{"clearcoatNormalUvScale", &Material::clearcoatNormalUvScale},
        std::pair{"clearcoatNormalUvOffsetU", &Material::clearcoatNormalUvOffsetU},
        std::pair{"clearcoatNormalUvOffsetV", &Material::clearcoatNormalUvOffsetV},
        std::pair{"sheenColorTextureUvScaleU",
                  &Material::sheenColorTextureUvScaleU},
        std::pair{"sheenColorTextureUvScaleV",
                  &Material::sheenColorTextureUvScaleV},
        std::pair{"sheenColorTextureUvRotationDegrees",
                  &Material::sheenColorTextureUvRotationDegrees},
        std::pair{"sheenColorTextureUvOffsetU",
                  &Material::sheenColorTextureUvOffsetU},
        std::pair{"sheenColorTextureUvOffsetV",
                  &Material::sheenColorTextureUvOffsetV},
        std::pair{"sheenRoughnessTextureUvScaleU",
                  &Material::sheenRoughnessTextureUvScaleU},
        std::pair{"sheenRoughnessTextureUvScaleV",
                  &Material::sheenRoughnessTextureUvScaleV},
        std::pair{"sheenRoughnessTextureUvRotationDegrees",
                  &Material::sheenRoughnessTextureUvRotationDegrees},
        std::pair{"sheenRoughnessTextureUvOffsetU",
                  &Material::sheenRoughnessTextureUvOffsetU},
        std::pair{"sheenRoughnessTextureUvOffsetV",
                  &Material::sheenRoughnessTextureUvOffsetV},
        std::pair{"anisotropyTextureUvScaleU",
                  &Material::anisotropyTextureUvScaleU},
        std::pair{"anisotropyTextureUvScaleV",
                  &Material::anisotropyTextureUvScaleV},
        std::pair{"anisotropyTextureUvRotationDegrees",
                  &Material::anisotropyTextureUvRotationDegrees},
        std::pair{"anisotropyTextureUvOffsetU",
                  &Material::anisotropyTextureUvOffsetU},
        std::pair{"anisotropyTextureUvOffsetV",
                  &Material::anisotropyTextureUvOffsetV},
        std::pair{"metallicTextureUvScaleU", &Material::metallicTextureUvScaleU},
        std::pair{"metallicTextureUvScaleV", &Material::metallicTextureUvScaleV},
        std::pair{"metallicTextureUvRotationDegrees",
                  &Material::metallicTextureUvRotationDegrees},
        std::pair{"metallicTextureUvOffsetU", &Material::metallicTextureUvOffsetU},
        std::pair{"metallicTextureUvOffsetV", &Material::metallicTextureUvOffsetV},
        std::pair{"roughnessTextureUvScaleU", &Material::roughnessTextureUvScaleU},
        std::pair{"roughnessTextureUvScaleV", &Material::roughnessTextureUvScaleV},
        std::pair{"roughnessTextureUvRotationDegrees",
                  &Material::roughnessTextureUvRotationDegrees},
        std::pair{"roughnessTextureUvOffsetU", &Material::roughnessTextureUvOffsetU},
        std::pair{"roughnessTextureUvOffsetV", &Material::roughnessTextureUvOffsetV},
        std::pair{"normalUvRotationDegrees", &Material::normalUvRotationDegrees},
        std::pair{"normalUvScale", &Material::normalUvScale},
        std::pair{"normalUvOffsetU", &Material::normalUvOffsetU},
        std::pair{"normalUvOffsetV", &Material::normalUvOffsetV},
        std::pair{"occlusionTextureUvScale", &Material::occlusionTextureUvScale},
        std::pair{"occlusionTextureUvOffsetU", &Material::occlusionTextureUvOffsetU},
        std::pair{"occlusionTextureUvOffsetV", &Material::occlusionTextureUvOffsetV},
        std::pair{"occlusionTextureUvRotationDegrees",
                  &Material::occlusionTextureUvRotationDegrees},
        std::pair{"uvOffsetU", &Material::uvOffsetU},
        std::pair{"uvOffsetV", &Material::uvOffsetV},
        std::pair{"uvScaleU", &Material::uvScaleU},
        std::pair{"uvScaleV", &Material::uvScaleV},
        std::pair{"uvRotationDegrees", &Material::uvRotationDegrees},
        std::pair{"emissionTextureUvScale", &Material::emissionTextureUvScale},
        std::pair{"emissionTextureUvOffsetU", &Material::emissionTextureUvOffsetU},
        std::pair{"emissionTextureUvOffsetV", &Material::emissionTextureUvOffsetV},
        std::pair{"emissionTextureUvRotationDegrees",
                  &Material::emissionTextureUvRotationDegrees},
        std::pair{"opacityTextureUvScale", &Material::opacityTextureUvScale},
        std::pair{"opacityTextureUvOffsetU", &Material::opacityTextureUvOffsetU},
        std::pair{"opacityTextureUvOffsetV", &Material::opacityTextureUvOffsetV},
        std::pair{"opacityTextureUvRotationDegrees",
                  &Material::opacityTextureUvRotationDegrees},
    };
    for (const auto& [fieldName, member] : nonFiniteScalars) {
        Material candidate = defaultMaterial;
        candidate.*member = nan;
        EXPECT_FALSE(manager_->setInstancePbrMaterial(
            ArtifactCore::UniString("box-object"), candidate))
            << "non-finite " << fieldName << " should sanitize to the default";
    }

    const std::array nonFiniteColorArrays{
        std::pair{"baseColor", &Material::baseColor},
        std::pair{"specularTint", &Material::specularTint},
        std::pair{"clearcoatTint", &Material::clearcoatTint},
        std::pair{"sheenTint", &Material::sheenTint},
        std::pair{"subsurfaceRadius", &Material::subsurfaceRadius},
        std::pair{"subsurfaceColor", &Material::subsurfaceColor},
        std::pair{"emissionColor", &Material::emissionColor},
    };
    for (const auto& [fieldName, member] : nonFiniteColorArrays) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            Material candidate = defaultMaterial;
            (candidate.*member)[channel] = nan;
            EXPECT_FALSE(manager_->setInstancePbrMaterial(
                ArtifactCore::UniString("box-object"), candidate))
                << "non-finite " << fieldName << " channel " << channel
                << " should sanitize to the default";
        }
    }
    EXPECT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("box-object"), boxPbrMaterial));

    std::array<ArtifactCore::RTSceneLight, 9> ambientLights{};
    for (auto& light : ambientLights) {
        light.positionType[3] =
            static_cast<float>(ArtifactCore::LightType::Ambient);
        light.directionIntensity[3] = 0.05f;
        light.colorAttenuationConstant[1] = 1.0f;
        light.colorAttenuationConstant[3] = 1.0f;
    }
    manager_->setSceneLights(context_.RawPtr(), ambientLights.data(),
                             static_cast<Uint32>(ambientLights.size()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto cappedSceneLightsImage = readOutputPixels();
    manager_->setSceneLights(context_.RawPtr(), ambientLights.data(), 8);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_LE(maximumGreenDifference(cappedSceneLightsImage), 1.0e-5f)
        << "ray tracing should use only the first eight scene lights";

    auto negativeIntensityLight = ambientLights[0];
    negativeIntensityLight.directionIntensity[3] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &negativeIntensityLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto negativeIntensityImage = readOutputPixels();
    negativeIntensityLight.directionIntensity[3] = 0.0f;
    manager_->setSceneLights(context_.RawPtr(), &negativeIntensityLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_LE(maximumGreenDifference(negativeIntensityImage), 1.0e-5f)
        << "negative scene-light intensity should clamp to zero";

    auto pointReferenceLight = greenPointLight;
    pointReferenceLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &pointReferenceLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto pointLightImage = readOutputPixels();
    auto attenuatedPointLight = pointReferenceLight;
    attenuatedPointLight.attenuationSpot[0] = 2.0f;
    manager_->setSceneLights(context_.RawPtr(), &attenuatedPointLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_GT(maximumGreenDifference(pointLightImage), 0.01f)
        << "linear point-light attenuation should reduce illumination with distance";
    auto zeroSizedAreaLight = greenAreaLight;
    zeroSizedAreaLight.shadowParams[0] = -1.0f;
    zeroSizedAreaLight.shadowParams[1] = 0.0f;
    zeroSizedAreaLight.shadowParams[2] = 0.0f;
    manager_->setSceneLights(context_.RawPtr(), &zeroSizedAreaLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_LE(maximumGreenDifference(pointLightImage), 1.0e-5f)
        << "an area light with zero dimensions should use the point-light path";

    auto rectangularAreaLight = greenAreaLight;
    rectangularAreaLight.shadowParams[0] = -1.0f;
    rectangularAreaLight.shadowParams[3] = 0.0f;
    manager_->setSceneLights(context_.RawPtr(), &rectangularAreaLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto rectangularAreaImage = readOutputPixels();
    auto diskAreaLight = rectangularAreaLight;
    diskAreaLight.shadowParams[3] = 1.0f;
    manager_->setSceneLights(context_.RawPtr(), &diskAreaLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_GT(maximumGreenDifference(rectangularAreaImage), 0.001f)
        << "disk and rectangular area-light sampling should produce distinct illumination";

    auto coneReferenceLight = greenPointLight;
    coneReferenceLight.shadowParams[0] = -1.0f;
    manager_->setSceneLights(context_.RawPtr(), &coneReferenceLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto pointInsideConePixel = pixelAt(24, 16);
    auto innerConeLight = coneReferenceLight;
    innerConeLight.positionType[3] =
        static_cast<float>(ArtifactCore::LightType::Spot);
    innerConeLight.directionIntensity[2] = 1.0f;
    innerConeLight.attenuationSpot[2] = 0.8f;
    innerConeLight.attenuationSpot[3] = 0.7f;
    manager_->setSceneLights(context_.RawPtr(), &innerConeLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    EXPECT_NEAR(pixelAt(24, 16)[1], pointInsideConePixel[1], 0.01f)
        << "a spot sample inside the inner cone should retain full point-light intensity";

    innerConeLight.attenuationSpot[2] = 0.95f;
    innerConeLight.attenuationSpot[3] = 0.9f;
    manager_->setSceneLights(context_.RawPtr(), &innerConeLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const float outsideConeGreen = pixelAt(24, 16)[1];
    EXPECT_LT(outsideConeGreen, pointInsideConePixel[1] - 0.01f)
        << "a spot sample outside the outer cone should be attenuated";

    innerConeLight.attenuationSpot[2] = 0.9f;
    innerConeLight.attenuationSpot[3] = 0.8f;
    manager_->setSceneLights(context_.RawPtr(), &innerConeLight, 1);
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(),
                                        kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const float transitionConeGreen = pixelAt(24, 16)[1];
    EXPECT_GT(transitionConeGreen, outsideConeGreen + 0.01f)
        << "a spot sample in the cone transition band should regain intensity";
    EXPECT_LT(transitionConeGreen, pointInsideConePixel[1] - 0.01f)
        << "a spot sample in the cone transition band should remain below full intensity";
}

TEST_F(RayTracingAcceptanceTest,
       ClampsCameraVerticalFieldOfViewToSupportedRange) {
    constexpr std::array<float, 12> vertices{
        -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, -1.0f, 1.0f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto vertexBuffer = makeBuffer(
        vertices.data(), sizeof(vertices), 3u * sizeof(float),
        BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    auto indexBuffer = makeBuffer(
        indices.data(), sizeof(indices), sizeof(std::uint32_t),
        BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(vertexBuffer, nullptr);
    ASSERT_NE(indexBuffer, nullptr);
    addGeometry("camera-fov-clamp", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.0f;
    material.emissionColor[0] = material.emissionColor[1] =
        material.emissionColor[2] = 1.0f;
    material.emissionStrength = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("camera-fov-clamp"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));

    const auto traceFov = [&](const float verticalFovDegrees) {
        ArtifactCore::RTRayCamera camera;
        camera.verticalFovDegrees = verticalFovDegrees;
        EXPECT_TRUE(manager_->traceCamera(context_.RawPtr(),
                                          kRayTraceTestImageSize,
                                          kRayTraceTestImageSize, camera));
        return readOutputPixels();
    };
    const auto belowMinimum = traceFov(0.5f);
    const auto atMinimum = traceFov(1.0f);
    const auto atMaximum = traceFov(179.0f);
    const auto aboveMaximum = traceFov(240.0f);
    const auto expectedPixelCount = static_cast<size_t>(
        kRayTraceTestImageSize * kRayTraceTestImageSize);
    ASSERT_EQ(belowMinimum.size(), expectedPixelCount);
    ASSERT_EQ(atMinimum.size(), expectedPixelCount);
    ASSERT_EQ(atMaximum.size(), expectedPixelCount);
    ASSERT_EQ(aboveMaximum.size(), expectedPixelCount);

    const auto maximumImageDifference = [](
        const std::vector<std::array<float, 4>>& lhs,
        const std::vector<std::array<float, 4>>& rhs) {
        EXPECT_EQ(lhs.size(), rhs.size());
        const auto count = std::min(lhs.size(), rhs.size());
        float maximumDifference = 0.0f;
        for (size_t pixel = 0; pixel < count; ++pixel) {
            for (size_t channel = 0; channel < 4; ++channel) {
                maximumDifference = std::max(
                    maximumDifference,
                    std::fabs(lhs[pixel][channel] - rhs[pixel][channel]));
            }
        }
        return maximumDifference;
    };
    EXPECT_LE(maximumImageDifference(belowMinimum, atMinimum), 1.0e-5f)
        << "FOV values below 1 degree should match the 1 degree lower bound";
    EXPECT_LE(maximumImageDifference(atMaximum, aboveMaximum), 1.0e-5f)
        << "FOV values above 179 degrees should match the 179 degree upper bound";
}

TEST_F(RayTracingAcceptanceTest, TracesUnindexedTriangleAndRejectsMalformedGeometry) {
    constexpr std::array<float, 9> triangleVertices{-0.8f, -0.8f, 0.0f, 0.8f, -0.8f,
                                                    0.0f,  0.0f,  0.8f, 0.0f};
    auto triangleBuffer =
        makeBuffer(triangleVertices.data(), sizeof(triangleVertices), 3u * sizeof(float),
                   BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(triangleBuffer, nullptr);
    const std::array<std::uint32_t, 3> shadingTriangleIndices{0, 1, 2};
    ArtifactCore::RTGeometryData unindexedGeometry;
    unindexedGeometry.pVertexBuffer = triangleBuffer.RawPtr();
    unindexedGeometry.vertexCount = 3;
    unindexedGeometry.geometryRevision = 1;
    unindexedGeometry.topologyRevision = 1;
    ASSERT_TRUE(manager_->createOrUpdateBLAS(ArtifactCore::UniString("unindexed-triangle"),
                                             unindexedGeometry));
    ASSERT_TRUE(manager_->updateShadingGeometry(ArtifactCore::UniString("unindexed-triangle"),
                                                triangleVertices.data(), 3,
                                                shadingTriangleIndices.data(), 3));
    ArtifactCore::RTGeometryData malformed;
    malformed.pVertexBuffer = triangleBuffer;
    malformed.vertexCount = 2;
    EXPECT_FALSE(
        manager_->createOrUpdateBLAS(ArtifactCore::UniString("too-few-vertices"), malformed));
    constexpr std::array<std::uint32_t, 3> triangleIndicesData{0, 1, 2};
    auto triangleIndices = makeBuffer(triangleIndicesData.data(), sizeof(triangleIndicesData),
        sizeof(std::uint32_t), BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    ASSERT_NE(triangleIndices, nullptr);
    malformed.vertexCount = 3;
    malformed.pIndexBuffer = triangleIndices;
    malformed.indexCount = 2;
    EXPECT_FALSE(
        manager_->createOrUpdateBLAS(ArtifactCore::UniString("incomplete-triangle"), malformed));
    malformed.vertexCount = 3;
    malformed.geometryRevision = 1;
    malformed.topologyRevision = 1;
    malformed.pVertexBuffer = triangleBuffer.RawPtr();
    malformed.vertexCount = 3;
    malformed.pIndexBuffer = nullptr;
    malformed.indexCount = 0;
    malformed.geometryRevision = 1;
    malformed.topologyRevision = 1;
    ASSERT_TRUE(manager_->createOrUpdateBLAS(ArtifactCore::UniString("unindexed-triangle"), malformed));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(
        manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize, kRayTraceTestImageSize));
    ASSERT_TRUE(saveOutputFrame("07-unindexed-triangle.png"));
    expectHit(16);
    EXPECT_EQ(manager_->capabilities().registeredBLASCount, 1u);
}
} // namespace
// namespace
