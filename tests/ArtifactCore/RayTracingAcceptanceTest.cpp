#include <BasicMath.hpp>
#include <Buffer.h>
#include <DeviceContext.h>
#include <EngineFactoryVk.h>
#include <QColor>
#include <QImage>
#include <QString>
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
import IO.ImageImporter;
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
}

TEST_F(RayTracingAcceptanceTest, SelectsPbrMaterialPerTriangleSlot) {
    constexpr std::array<float, 18> vertices{
        -0.9f, -0.9f, 0.0f, 0.0f, -0.9f, 0.0f, -0.45f, 0.9f, 0.0f,
         0.0f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.45f, 0.9f, 0.0f};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
    constexpr std::array<std::uint32_t, 2> materialIndices{0, 1};
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
        indices.data(), 6, nullptr, materialIndices.data()));

    ArtifactCore::RTPbrMaterial leftMaterial;
    leftMaterial.baseColor[0] = 0.0f;
    leftMaterial.baseColor[1] = 0.0f;
    leftMaterial.baseColor[2] = 0.0f;
    leftMaterial.emissionColor[0] = 1.0f;
    leftMaterial.emissionStrength = 1.0f;
    ArtifactCore::RTPbrMaterial rightMaterial = leftMaterial;
    rightMaterial.emissionColor[0] = 0.0f;
    rightMaterial.emissionColor[2] = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("material-slot-selection"), leftMaterial, 0));
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
}

TEST_F(RayTracingAcceptanceTest, SamplesOcclusionTextureWithInterpolatedMeshUvs) {
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
    addGeometry("ao-uv-sampling", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("ao-uv-sampling"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data()));

    const auto texturePath = std::filesystem::temp_directory_path() /
                             "artifact-rt-ao-uv-test.png";
    QImage aoImage(8, 2, QImage::Format_Grayscale8);
    ASSERT_FALSE(aoImage.isNull());
    for (int y = 0; y < aoImage.height(); ++y) {
        auto* row = aoImage.scanLine(y);
        std::fill(row, row + 4, static_cast<uchar>(0));
        std::fill(row + 4, row + 8, static_cast<uchar>(255));
    }
    ASSERT_TRUE(aoImage.save(QString::fromStdString(texturePath.string()), "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.7f;
    material.occlusionTexture = ArtifactCore::UniString(
        QString::fromStdString(texturePath.string()));
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
    std::error_code ignored;
    std::filesystem::remove(texturePath, ignored);
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

    const auto tempDirectory = std::filesystem::temp_directory_path();
    const auto baseColorPath = tempDirectory / "artifact-rt-basecolor-map.png";
    const auto metallicMapPath = tempDirectory / "artifact-rt-metallic-map.png";
    const auto dielectricMapPath = tempDirectory / "artifact-rt-dielectric-map.png";
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
    const auto dielectricRight = pixelAt(24, 16);
    const float rightMapDifference = std::max(
        std::fabs(dielectricRight[0] - mappedMetallicRight[0]),
        std::max(std::fabs(dielectricRight[1] - mappedMetallicRight[1]),
                 std::fabs(dielectricRight[2] - mappedMetallicRight[2])));
    EXPECT_GT(rightMapDifference, 0.01f);
    std::error_code ignored;
    std::filesystem::remove(baseColorPath, ignored);
    std::filesystem::remove(metallicMapPath, ignored);
    std::filesystem::remove(dielectricMapPath, ignored);
}

TEST_F(RayTracingAcceptanceTest, SamplesSrgbEmissionTexture) {
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

    const auto texturePath = std::filesystem::temp_directory_path() /
                             "artifact-rt-emission-texture.png";
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
    ASSERT_TRUE(emissionImage.save(
        QString::fromStdString(texturePath.string()), "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.0f;
    material.specular = 0.0f;
    material.ior = 1.0f;
    material.emissionColor[0] = material.emissionColor[1] =
        material.emissionColor[2] = 1.0f;
    material.emissionStrength = 1.0f;
    material.emissionTexture = ArtifactCore::UniString(
        QString::fromStdString(texturePath.string()));
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
    std::error_code ignored;
    std::filesystem::remove(texturePath, ignored);
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
        indices.data(), 12, nullptr, materialIndices.data(), uvs.data()));

    const auto texturePath = std::filesystem::temp_directory_path() /
                             "artifact-rt-masked-opacity.png";
    QImage opacityImage(8, 2, QImage::Format_RGBA8888);
    ASSERT_FALSE(opacityImage.isNull());
    for (int y = 0; y < opacityImage.height(); ++y) {
        auto* row = opacityImage.scanLine(y);
        for (int x = 0; x < opacityImage.width(); ++x) {
            auto* pixel = row + x * 4;
            const uchar value = x < 4 ? 0 : 255;
            pixel[0] = pixel[1] = pixel[2] = value;
            pixel[3] = 255;
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
    std::error_code ignored;
    std::filesystem::remove(texturePath, ignored);
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
        4, blockerIndices.data(), 6, nullptr, nullptr, blockerUvs.data()));

    ArtifactCore::RTPbrMaterial receiverMaterial;
    receiverMaterial.baseColor[0] = receiverMaterial.baseColor[1] =
        receiverMaterial.baseColor[2] = 0.8f;
    receiverMaterial.roughness = 1.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("masked-shadow-receiver"), receiverMaterial));

    const auto tempDirectory = std::filesystem::temp_directory_path();
    const auto whiteMapPath = tempDirectory / "artifact-rt-shadow-opacity-white.png";
    const auto blackMapPath = tempDirectory / "artifact-rt-shadow-opacity-black.png";
    for (const auto& item : std::array<std::pair<std::filesystem::path, QColor>, 2>{
             std::pair{whiteMapPath, QColor(255, 255, 255, 255)},
             std::pair{blackMapPath, QColor(0, 0, 0, 255)}}) {
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
        QString::fromStdString(blackMapPath.string()));
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

    std::error_code ignored;
    std::filesystem::remove(whiteMapPath, ignored);
    std::filesystem::remove(blackMapPath, ignored);
}

TEST_F(RayTracingAcceptanceTest, SelectsMipLevelsForMinifiedPbrTextures) {
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

    constexpr int textureSize = 128;
    const auto tempDirectory = std::filesystem::temp_directory_path();
    const auto checkerPath = tempDirectory / "artifact-rt-pbr-minified-checker.png";
    const auto averagePath = tempDirectory / "artifact-rt-pbr-minified-average.png";
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
    ASSERT_TRUE(checker.save(QString::fromStdString(checkerPath.string()), "PNG"));
    ASSERT_TRUE(average.save(QString::fromStdString(averagePath.string()), "PNG"));

    ArtifactCore::RTPbrMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 1.0f;
    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(checkerPath.string()));
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

    material.baseColorTexture = ArtifactCore::UniString(
        QString::fromStdString(averagePath.string()));
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
    std::error_code ignored;
    std::filesystem::remove(checkerPath, ignored);
    std::filesystem::remove(averagePath, ignored);
}

TEST_F(RayTracingAcceptanceTest, AppliesNormalTextureInPerTriangleTangentSpace) {
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
    addGeometry("pbr-normal-map", vertexBuffer.RawPtr(), 4,
                indexBuffer.RawPtr(), 6, float4x4::Identity(), 1, 1,
                vertices.data(), indices.data());
    ASSERT_TRUE(manager_->updateShadingGeometry(
        ArtifactCore::UniString("pbr-normal-map"), vertices.data(), 4,
        indices.data(), 6, nullptr, nullptr, uvs.data()));

    const auto normalPath = std::filesystem::temp_directory_path() /
                            "artifact-rt-normal-map.png";
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

    material.normalStrength = 0.0f;
    ASSERT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("pbr-normal-map"), material));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), kRayTraceTestImageSize,
                                        kRayTraceTestImageSize));
    const auto disabledRight = pixelAt(24, 16);
    EXPECT_NEAR(disabledRight[0], flatRight[0], 0.01f);
    std::error_code ignored;
    std::filesystem::remove(normalPath, ignored);
}

TEST_F(RayTracingAcceptanceTest, ClearcoatRoughnessUsesMeshRendererEnvironmentPrefilter) {
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

    const auto aoOutputDir = std::filesystem::current_path() / "ray-tracing-frames";
    std::filesystem::create_directories(aoOutputDir);
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
            const auto aoTexturePath = aoOutputDir /
                ("mesh-renderer-ao-" + std::to_string(aoValue) + ".png");
            ASSERT_TRUE(aoImage.save(
                QString::fromStdString(aoTexturePath.string()), "PNG"));
            renderer.setOcclusionTexture(
                QString::fromStdString(aoTexturePath.string()));
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
    const auto spatialAoTexturePath = aoOutputDir / "mesh-renderer-spatial-ao.png";
    ASSERT_TRUE(spatialAoImage.save(
        QString::fromStdString(spatialAoTexturePath.string()), "PNG"));
    renderer.setPbrFactors(0.0f, 0.2f, 1.0f, 1.0f);
    const auto leftUnoccluded = renderPixelAt(20, height / 2);
    const auto rightUnoccluded = renderPixelAt(44, height / 2);
    renderer.setOcclusionTexture(
        QString::fromStdString(spatialAoTexturePath.string()));
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
    renderer.setOcclusionTexture(
        QString::fromStdString(spatialAoTexturePath.string()));
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

    const char* configuredOutputDir = std::getenv("ARTIFACT_RT_FRAME_OUTPUT_DIR");
    const auto outputDir = configuredOutputDir && configuredOutputDir[0] != '\0'
        ? std::filesystem::path(configuredOutputDir)
        : std::filesystem::current_path() / "ray-tracing-frames";
    std::filesystem::create_directories(outputDir);
    const auto hdrPath = outputDir / "mesh-renderer-hdr-source.hdr";
    std::ofstream hdrFile(hdrPath, std::ios::binary);
    ASSERT_TRUE(hdrFile.is_open());
    hdrFile << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 4 +X 8\n";
    constexpr std::array<std::uint8_t, 4> rgbePixel{128, 32, 8, 131};
    for (int y = 0; y < 4; ++y) {
        constexpr std::array<std::uint8_t, 4> scanlineHeader{2, 2, 0, 8};
        hdrFile.write(reinterpret_cast<const char*>(scanlineHeader.data()),
                       static_cast<std::streamsize>(scanlineHeader.size()));
        for (std::uint8_t channel : rgbePixel) {
            const std::array<std::uint8_t, 2> run{136, channel};
            hdrFile.write(reinterpret_cast<const char*>(run.data()),
                           static_cast<std::streamsize>(run.size()));
        }
    }
    hdrFile.close();
    ASSERT_TRUE(hdrFile.good());

    ArtifactCore::ImageImporter hdrImporter;
    ASSERT_TRUE(hdrImporter.open(QString::fromStdString(hdrPath.string())));
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

    const auto hdrPathString = QString::fromStdString(hdrPath.string());
    ASSERT_TRUE(renderer.setEnvironmentMap(hdrPathString, 1.0f));
    ASSERT_TRUE(renderer.hasEnvironmentMap());
    instance.color[0] = 1.0f;
    instance.color[1] = 0.0f;
    instance.color[2] = 0.0f;
    renderer.updateInstanceData(&instance, 1);
    renderer.setPbrFactors(1.0f, 0.04f, 1.0f, 1.0f);
    renderer.setPrincipledFactors(0.5f, 1.5f, 0.0f, 0.0f, 0.04f);
    const auto hdrLitPixel = renderCenterPixel();
    ASSERT_TRUE(renderer.setEnvironmentMap(hdrPathString, 0.25f));
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
    };
    constexpr std::array<MaterialProbe, 9> materialProbes{{
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
         {1.0f, 1.0f, 1.0f}, 1.0f, 0.2f}}};
    std::array<std::array<float, 4>, materialProbes.size()> rasterProbePixels{};
    std::array<std::array<float, 4>, materialProbes.size()> rtProbePixels{};
    for (size_t probeIndex = 0; probeIndex < materialProbes.size();
         ++probeIndex) {
        const auto& probe = materialProbes[probeIndex];
        instance.color[0] = probe.baseColor[0];
        instance.color[1] = probe.baseColor[1];
        instance.color[2] = probe.baseColor[2];
        renderer.updateInstanceData(&instance, 1);
        renderer.setPbrFactors(probe.metallic, probe.roughness, 1.0f, 1.0f);
        renderer.setPrincipledFactors(0.5f, probe.ior, 0.0f,
                                      probe.clearcoat,
                                      probe.clearcoatRoughness, probe.sheen);
        renderer.setSheenFactors(probe.sheenRoughness,
                                 QColor::fromRgbF(probe.sheenTint[0],
                                                  probe.sheenTint[1],
                                                  probe.sheenTint[2]));
        rasterProbePixels[probeIndex] = renderCenterPixel();

        ArtifactCore::RTPbrMaterial rtMaterial;
        rtMaterial.baseColor[0] = probe.baseColor[0];
        rtMaterial.baseColor[1] = probe.baseColor[1];
        rtMaterial.baseColor[2] = probe.baseColor[2];
        rtMaterial.metallic = probe.metallic;
        rtMaterial.roughness = probe.roughness;
        rtMaterial.specular = 0.5f;
        rtMaterial.ior = probe.ior;
        rtMaterial.sheen = probe.sheen;
        rtMaterial.clearcoat = probe.clearcoat;
        rtMaterial.clearcoatRoughness = probe.clearcoatRoughness;
        rtMaterial.sheenRoughness = probe.sheenRoughness;
        std::copy(probe.sheenTint.begin(), probe.sheenTint.end(),
                  rtMaterial.sheenTint);
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
            const float parityTolerance = probeIndex >= 5 && probeIndex < 8
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
    for (size_t channel = 0; channel < 3; ++channel) {
        sheenTintDifference += std::fabs(
            rasterProbePixels[5][channel] - rasterProbePixels[7][channel]);
    }
    EXPECT_GT(sheenTintDifference, 0.005f);

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
    ArtifactCore::RTPbrMaterial nonFiniteMaterial;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    nonFiniteMaterial.baseColor[0] = nan;
    nonFiniteMaterial.metallic = infinity;
    nonFiniteMaterial.roughness = nan;
    nonFiniteMaterial.ior = nan;
    nonFiniteMaterial.emissionStrength = infinity;
    EXPECT_FALSE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("box-object"), nonFiniteMaterial));
    EXPECT_TRUE(manager_->setInstancePbrMaterial(
        ArtifactCore::UniString("box-object"), boxPbrMaterial));
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
