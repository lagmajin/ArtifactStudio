#include <gtest/gtest.h>

#include <Buffer.h>
#include <DeviceContext.h>
#include <EngineFactoryVk.h>
#include <RefCntAutoPtr.hpp>
#include <Texture.h>
#include <TopLevelAS.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

import Graphics.RayTracingManager;
import Utils.String.UniString;

namespace {

using namespace Diligent;

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

    void addGeometry(const char* id, IBuffer* vertices, Uint32 vertexCount,
                     IBuffer* indices, Uint32 indexCount,
                     const float4x4& transform = float4x4::Identity(),
                     Uint64 geometryRevision = 1,
                     Uint64 topologyRevision = 1) {
        ArtifactCore::RTGeometryData geometry;
        geometry.pVertexBuffer = vertices;
        geometry.vertexCount = vertexCount;
        geometry.pIndexBuffer = indices;
        geometry.indexCount = indexCount;
        geometry.transform = transform;
        geometry.geometryRevision = geometryRevision;
        geometry.topologyRevision = topologyRevision;
        ASSERT_TRUE(manager_->createOrUpdateBLAS(ArtifactCore::UniString(id), geometry));
    }

    bool updateBuffer(IBuffer* buffer, const void* data, Uint64 size) {
        if (!buffer || !context_) return false;
        context_->UpdateBuffer(buffer, 0, size, data,
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        return true;
    }

    std::array<float, 4> pixelAt(Uint32 x, Uint32 y) {
        constexpr Uint32 width = 32;
        constexpr Uint32 height = 32;
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

        CopyTextureAttribs copy(output, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                staging, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context_->CopyTexture(copy);
        context_->Flush();
        context_->WaitForIdle();

        MappedTextureSubresource mapped{};
        context_->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE,
                                        nullptr, mapped);
        EXPECT_NE(mapped.pData, nullptr);
        if (!mapped.pData) return {};
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.Stride;
        std::array<float, 4> pixel{};
        std::memcpy(pixel.data(), row + x * sizeof(pixel), sizeof(pixel));
        context_->UnmapTextureSubresource(staging, 0, 0);
        return pixel;
    }

    void expectHit(Uint32 x, Uint32 y = 16) {
        const auto pixel = pixelAt(x, y);
        EXPECT_NEAR(pixel[0], 0.9f, 0.03f);
        EXPECT_NEAR(pixel[1], 0.75f, 0.03f);
        EXPECT_NEAR(pixel[2], 0.2f, 0.03f);
        EXPECT_NEAR(pixel[3], 1.0f, 0.03f);
    }

    void expectMiss(Uint32 x, Uint32 y = 16) {
        const auto pixel = pixelAt(x, y);
        EXPECT_NEAR(pixel[0], 0.2f, 0.03f);
        EXPECT_NEAR(pixel[1], 0.45f, 0.03f);
        EXPECT_NEAR(pixel[2], 0.9f, 0.03f);
        EXPECT_NEAR(pixel[3], 1.0f, 0.03f);
    }

    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IDeviceContext> context_;
    std::unique_ptr<ArtifactCore::IRayTracingManager> manager_;
};

TEST_F(RayTracingAcceptanceTest, TracesPlaneAndThreeDimensionalMeshGeometry) {
    // A flat layer-like quad on the left half of the ray field.
    constexpr std::array<float, 12> planeVertices{
        -0.95f, -0.75f, 0.0f, -0.05f, -0.75f, 0.0f,
        -0.05f,  0.75f, 0.0f, -0.95f,  0.75f, 0.0f};
    constexpr std::array<std::uint32_t, 6> planeIndices{0, 1, 2, 0, 2, 3};

    // A closed box mesh on the right half, with front/back and side triangles.
    constexpr std::array<float, 24> boxVertices{
        -0.45f, -0.65f, -0.30f, 0.45f, -0.65f, -0.30f,
         0.45f,  0.65f, -0.30f, -0.45f,  0.65f, -0.30f,
        -0.45f, -0.65f,  0.30f, 0.45f, -0.65f,  0.30f,
         0.45f,  0.65f,  0.30f, -0.45f,  0.65f,  0.30f};
    constexpr std::array<std::uint32_t, 36> boxIndices{
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};

    auto planeVertexBuffer = makeBuffer(planeVertices.data(), sizeof(planeVertices),
        3u * sizeof(float), BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE,
        BUFFER_MODE_STRUCTURED);
    auto planeIndexBuffer = makeBuffer(planeIndices.data(), sizeof(planeIndices), 0,
                                       BIND_INDEX_BUFFER);
    auto boxVertexBuffer = makeBuffer(boxVertices.data(), sizeof(boxVertices),
        3u * sizeof(float), BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE,
        BUFFER_MODE_STRUCTURED);
    auto boxIndexBuffer = makeBuffer(boxIndices.data(), sizeof(boxIndices), 0,
                                     BIND_INDEX_BUFFER);
    ASSERT_NE(planeVertexBuffer, nullptr);
    ASSERT_NE(planeIndexBuffer, nullptr);
    ASSERT_NE(boxVertexBuffer, nullptr);
    ASSERT_NE(boxIndexBuffer, nullptr);

    addGeometry("plane-layer", planeVertexBuffer.RawPtr(), 4,
                planeIndexBuffer.RawPtr(), 6);
    auto boxTransform = float4x4::Identity();
    boxTransform.m30 = 0.5f;
    addGeometry("box-object", boxVertexBuffer.RawPtr(), 8,
                boxIndexBuffer.RawPtr(), 36, boxTransform);
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), 32, 32));
    context_->Flush();
    context_->WaitForIdle();

    expectMiss(0);   // Outside both geometries.
    expectHit(8);    // Plane layer.
    expectMiss(16);  // Gap between plane and 3D object.
    expectHit(24);   // 3D box object.

    auto scaledBoxTransform = float4x4::Identity();
    scaledBoxTransform.m00 = 0.5f;
    scaledBoxTransform.m11 = 0.5f;
    scaledBoxTransform.m22 = 0.5f;
    scaledBoxTransform.m30 = 0.5f;
    ASSERT_TRUE(manager_->updateInstanceTransform(
        ArtifactCore::UniString("box-object"), scaledBoxTransform));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), 32, 32));
    expectHit(24, 20);
    expectMiss(24, 22);  // The instance scale changes the hit bounds.
    expectMiss(29);      // The scaled object no longer reaches this column.

    const ArtifactCore::UniString boxId("box-object");
    ASSERT_TRUE(manager_->setInstanceActive(boxId, false));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), 32, 32));
    expectMiss(24);
    EXPECT_EQ(manager_->capabilities().activeInstanceCount, 1u);

    ASSERT_TRUE(manager_->setInstanceActive(boxId, true));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), 32, 32));
    expectHit(24, 20);

    constexpr std::array<float, 12> movedPlaneVertices{
        -0.45f, -0.75f, 0.0f, 0.45f, -0.75f, 0.0f,
         0.45f,  0.75f, 0.0f, -0.45f,  0.75f, 0.0f};
    ASSERT_TRUE(updateBuffer(planeVertexBuffer.RawPtr(), movedPlaneVertices.data(),
                             sizeof(movedPlaneVertices)));
    ArtifactCore::RTGeometryData updatedPlane;
    updatedPlane.pVertexBuffer = planeVertexBuffer.RawPtr();
    updatedPlane.vertexCount = 4;
    updatedPlane.pIndexBuffer = planeIndexBuffer.RawPtr();
    updatedPlane.indexCount = 6;
    updatedPlane.geometryRevision = 2;
    updatedPlane.topologyRevision = 1;
    ASSERT_TRUE(manager_->createOrUpdateBLAS(
        ArtifactCore::UniString("plane-layer"), updatedPlane));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), 32, 32));
    expectMiss(8);
    expectHit(16);
    EXPECT_EQ(manager_->capabilities().blasUpdateCount, 1u);

    // Restrict the plane to one triangle so the rebuild changes output.
    constexpr std::array<std::uint32_t, 3> changedPlaneIndices{0, 1, 2};
    ASSERT_TRUE(updateBuffer(planeIndexBuffer.RawPtr(), changedPlaneIndices.data(),
                             sizeof(changedPlaneIndices)));
    updatedPlane.indexCount = static_cast<Uint32>(changedPlaneIndices.size());
    updatedPlane.geometryRevision = 3;
    updatedPlane.topologyRevision = 2;
    ASSERT_TRUE(manager_->createOrUpdateBLAS(
        ArtifactCore::UniString("plane-layer"), updatedPlane));
    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), 32, 32));
    expectHit(18);
    expectMiss(13);
    EXPECT_EQ(manager_->capabilities().blasRebuildCount, 3u);

    const auto& capabilities = manager_->capabilities();
    EXPECT_EQ(capabilities.registeredBLASCount, 2u);
    EXPECT_EQ(capabilities.activeInstanceCount, 2u);
    EXPECT_EQ(capabilities.traceDispatchCount, 6u);
    EXPECT_GE(capabilities.tlasUpdateCount, 5u);
}

TEST_F(RayTracingAcceptanceTest, TracesUnindexedTriangleAndRejectsMalformedGeometry) {
    constexpr std::array<float, 9> triangleVertices{
        -0.8f, -0.8f, 0.0f, 0.8f, -0.8f, 0.0f, 0.0f, 0.8f, 0.0f};
    auto triangleBuffer = makeBuffer(triangleVertices.data(), sizeof(triangleVertices),
        3u * sizeof(float), BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE,
        BUFFER_MODE_STRUCTURED);
    ASSERT_NE(triangleBuffer, nullptr);
    addGeometry("unindexed-triangle", triangleBuffer.RawPtr(), 3, nullptr, 0);

    ArtifactCore::RTGeometryData malformed;
    malformed.pVertexBuffer = triangleBuffer;
    malformed.vertexCount = 2;
    EXPECT_FALSE(manager_->createOrUpdateBLAS(
        ArtifactCore::UniString("too-few-vertices"), malformed));

    constexpr std::array<std::uint32_t, 3> triangleIndicesData{0, 1, 2};
    auto triangleIndices = makeBuffer(triangleIndicesData.data(),
        sizeof(triangleIndicesData), 0, BIND_INDEX_BUFFER);
    ASSERT_NE(triangleIndices, nullptr);
    malformed.vertexCount = 3;
    malformed.pIndexBuffer = triangleIndices;
    malformed.indexCount = 2;
    EXPECT_FALSE(manager_->createOrUpdateBLAS(
        ArtifactCore::UniString("incomplete-triangle"), malformed));

    ASSERT_TRUE(manager_->buildTLAS(context_.RawPtr()));
    ASSERT_TRUE(manager_->ensurePipelineAndSBT(context_.RawPtr()));
    ASSERT_TRUE(manager_->traceUnitQuad(context_.RawPtr(), 32, 32));
    expectHit(16);
    EXPECT_EQ(manager_->capabilities().registeredBLASCount, 1u);
}

} // namespace
