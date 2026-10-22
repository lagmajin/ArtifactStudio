#include <DeviceContext.h>
#include <EngineFactoryVk.h>
#include <QVector3D>
#include <RefCntAutoPtr.hpp>
#include <Texture.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <vector>
import Graphics.GPUcomputeContext;
import Graphics.MeshRenderer;
import Mesh;

namespace {

using namespace Diligent;

class MeshShaderLodTest : public testing::Test {
  protected:
    void SetUp() override {
        auto* factory = LoadAndGetEngineFactoryVk();
        if (!factory) {
            GTEST_SKIP() << "Diligent Vulkan backend is unavailable";
        }
        EngineVkCreateInfo createInfo{};
        createInfo.Features.MultithreadedResourceCreation =
            DEVICE_FEATURE_STATE_DISABLED;
        createInfo.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
        factory->CreateDeviceAndContextsVk(createInfo, &device_, &context_);
        if (!device_ || !context_) {
            GTEST_SKIP() << "Could not create a headless Vulkan device";
        }
    }

    void TearDown() override {
        if (context_) {
            context_->Flush();
            context_->WaitForIdle();
        }
        context_.Release();
        device_.Release();
    }

    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IDeviceContext> context_;
};

TEST_F(MeshShaderLodTest, MeshRendererSelectsMeshletLodAtProjectedSizeBoundaries) {
    ArtifactCore::GpuContext gpuContext(device_.RawPtr(), context_.RawPtr());
    ArtifactCore::MeshRenderer renderer(gpuContext);
    EXPECT_EQ(renderer.chooseMeshletLOD(0.0f), 0u)
        << "Without uploaded levels, selection should fall back to level zero";

    ArtifactCore::Mesh sourceMesh;
    constexpr int cellsPerSide = 8;
    constexpr int verticesPerSide = cellsPerSide + 1;
    sourceMesh.setVertexCount(verticesPerSide * verticesPerSide);
    auto positions = sourceMesh.vertexAttributes().add<QVector3D>("position");
    for (int y = 0; y < verticesPerSide; ++y) {
        for (int x = 0; x < verticesPerSide; ++x) {
            const int index = y * verticesPerSide + x;
            (*positions)[index] = QVector3D(static_cast<float>(x),
                                            static_cast<float>(y), 0.0f);
        }
    }
    for (int y = 0; y < cellsPerSide; ++y) {
        for (int x = 0; x < cellsPerSide; ++x) {
            const int topLeft = y * verticesPerSide + x;
            sourceMesh.addPolygon({topLeft, topLeft + 1,
                                   topLeft + verticesPerSide + 1,
                                   topLeft + verticesPerSide});
        }
    }

    ArtifactCore::Mesh::MeshletLODConfig lodConfig;
    lodConfig.maxLODLevels = 4;
    lodConfig.maxTrianglesPerMeshlet = 8;
    lodConfig.lodSwitchPixels = 96.0f;
    const ArtifactCore::Mesh::MeshletLODData cpuLodData =
        sourceMesh.generateMeshletLODData(lodConfig);
    ASSERT_TRUE(cpuLodData.isValid());
    ASSERT_EQ(cpuLodData.levels.size(), 4);

    std::vector<ArtifactCore::MeshRenderer::MeshletGpu> meshlets(
        static_cast<std::size_t>(cpuLodData.meshlets.size()));
    std::vector<std::uint32_t> indices(
        static_cast<std::size_t>(cpuLodData.lodIndices.size()));
    std::vector<ArtifactCore::MeshRenderer::MeshletLodGpu> lods;
    lods.reserve(static_cast<std::size_t>(cpuLodData.levels.size()));
    for (const ArtifactCore::Mesh::MeshletLODLevel& level : cpuLodData.levels) {
        lods.push_back({static_cast<std::uint32_t>(level.meshletOffset),
                        static_cast<std::uint32_t>(level.meshletCount),
                        level.switchDistancePixels, 0});
    }
    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(),
                                   lods.data(), lods.size());

    struct ProjectedSizeCase {
        float radiusPixels;
        std::size_t expectedLevel;
    };
    const std::array<ProjectedSizeCase, 16> cases{{
        {std::numeric_limits<float>::infinity(), 0},
        {96.0f, 0},
        {std::nextafter(96.0f, std::numeric_limits<float>::infinity()), 0},
        {48.0f, 1},
        {std::nextafter(48.0f, 0.0f), 1},
        {std::nextafter(48.0f, 96.0f), 0},
        {24.0f, 2},
        {std::nextafter(24.0f, 0.0f), 2},
        {std::nextafter(24.0f, 48.0f), 1},
        {12.0f, 3},
        {std::nextafter(12.0f, 0.0f), 3},
        {std::nextafter(12.0f, 24.0f), 2},
        {0.0f, 3},
        {-1.0f, 3},
        {-std::numeric_limits<float>::infinity(), 3},
        {std::numeric_limits<float>::quiet_NaN(), 0},
    }};
    for (const auto& testCase : cases) {
        const std::size_t gpuLevel =
            renderer.chooseMeshletLOD(testCase.radiusPixels);
        const int cpuLevel = ArtifactCore::Mesh::chooseMeshletLODLevel(
            cpuLodData, testCase.radiusPixels);
        EXPECT_EQ(gpuLevel, testCase.expectedLevel)
            << "projected radius " << testCase.radiusPixels;
        EXPECT_EQ(cpuLevel, static_cast<int>(gpuLevel))
            << "CPU/GPU LOD selection differs at projected radius "
            << testCase.radiusPixels;
    }

    auto replacementLods = lods;
    replacementLods.resize(2);
    for (auto& level : replacementLods) {
        level.switchPixels = 1000.0f;
    }
    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(),
                                   replacementLods.data(), replacementLods.size());
    EXPECT_EQ(renderer.chooseMeshletLOD(0.0f), replacementLods.size() - 1)
        << "uploading a shorter LOD table should discard the old trailing levels";

    auto singleLevelLods = lods;
    singleLevelLods.resize(1);
    singleLevelLods[0].switchPixels = 24.0f;
    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(),
                                   singleLevelLods.data(), singleLevelLods.size());
    for (const float radiusPixels : {
             -std::numeric_limits<float>::infinity(), 0.0f, 24.0f, 1000.0f,
             std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()}) {
        EXPECT_EQ(renderer.chooseMeshletLOD(radiusPixels), 0u)
            << "a one-level GPU LOD table must always select its only level";
    }

    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(),
                                   replacementLods.data(), replacementLods.size());
    EXPECT_EQ(renderer.chooseMeshletLOD(0.0f), replacementLods.size() - 1)
        << "restoring a multi-level table should restore its coarsest selection";
    for (const auto& testCase : cases) {
        const std::size_t gpuLevel =
            renderer.chooseMeshletLOD(testCase.radiusPixels);
        const int cpuLevel = ArtifactCore::Mesh::chooseMeshletLODLevel(
            cpuLodData, testCase.radiusPixels);
        EXPECT_EQ(gpuLevel, testCase.expectedLevel)
            << "restored GPU LOD table selected the wrong level at projected radius "
            << testCase.radiusPixels;
        EXPECT_EQ(gpuLevel, static_cast<std::size_t>(cpuLevel))
            << "restored GPU LOD table diverged from CPU selection at projected radius "
            << testCase.radiusPixels;
    }

    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(),
                                   singleLevelLods.data(), 0);
    EXPECT_EQ(renderer.chooseMeshletLOD(0.0f), 0u)
        << "a zero LOD count must clear stale selections even with a non-null pointer";

    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(),
                                   lods.data(), lods.size());
    ASSERT_EQ(renderer.chooseMeshletLOD(0.0f), lods.size() - 1)
        << "restoring the uploaded LOD table should reinstate the coarsest selection";

    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(),
                                   nullptr, lods.size());
    EXPECT_EQ(renderer.chooseMeshletLOD(0.0f), 0u)
        << "a null LOD table must clear stale selections even when its count is nonzero";

    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   indices.data(), indices.size(), nullptr, 0);
    EXPECT_EQ(renderer.chooseMeshletLOD(0.0f), 0u)
        << "replacing uploaded geometry without LOD levels should clear the old selection table";
}

TEST_F(MeshShaderLodTest, MeshRendererDrawsTheSelectedMeshletLod) {
    ArtifactCore::GpuContext gpuContext(device_.RawPtr(), context_.RawPtr());
    ArtifactCore::MeshRenderer renderer(gpuContext);
    renderer.setRenderTargetFormat(TEX_FORMAT_RGBA32_FLOAT);
    renderer.initialize(1, 9, 0);

    // The fine LOD has two triangles in separate target bands; the coarse LOD
    // has one in the right band. Readback verifies both the selected LOD's
    // base offset and its meshlet draw count, beyond the CPU-side chooser.
    constexpr std::array<float, 27> positions{
        -0.95f, -0.8f, 0.0f, -0.65f, -0.8f, 0.0f, -0.80f, 0.8f, 0.0f,
        -0.35f, -0.8f, 0.0f, -0.05f, -0.8f, 0.0f, -0.20f, 0.8f, 0.0f,
         0.55f, -0.8f, 0.0f,  0.85f, -0.8f, 0.0f,  0.70f, 0.8f, 0.0f};
    constexpr std::array<std::uint32_t, 9> meshletIndices{
        0, 1, 2, 3, 4, 5, 6, 7, 8};
    std::array<ArtifactCore::MeshRenderer::MeshletGpu, 3> meshlets{};
    for (std::size_t index = 0; index < meshlets.size(); ++index) {
        meshlets[index].indexOffset = static_cast<std::uint32_t>(index * 3);
        meshlets[index].indexCount = 3;
        meshlets[index].vertexOffset = static_cast<std::uint32_t>(index * 3);
        meshlets[index].vertexCount = 3;
        meshlets[index].boundsRadius = 1.0f;
    }
    constexpr std::array<ArtifactCore::MeshRenderer::MeshletLodGpu, 2> lods{{
        {0, 2, 96.0f, 0},
        {2, 1, 48.0f, 0},
    }};
    renderer.updateMeshGeometry(positions.data(), nullptr, nullptr, nullptr);
    renderer.updateMeshletGeometry(meshlets.data(), meshlets.size(),
                                   meshletIndices.data(), meshletIndices.size(),
                                   lods.data(), lods.size());
    if (!renderer.meshShaderReady()) {
        GTEST_SKIP() << "The active Vulkan adapter does not provide a usable mesh-shader PSO";
    }

    constexpr std::array<float, 16> identity{
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    renderer.setMeshletMatrices(identity.data(), identity.data(), identity.data());

    constexpr Uint32 width = 64;
    constexpr Uint32 height = 64;
    TextureDesc colorDesc;
    colorDesc.Name = "MeshletLodDrawColorTarget";
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
    depthDesc.Name = "MeshletLodDrawDepthTarget";
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
    stagingDesc.Name = "MeshletLodDrawReadback";
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

    const auto countDrawnPixelsByBand = [&](float projectedRadiusPixels) {
        const auto lodIndex = renderer.chooseMeshletLOD(projectedRadiusPixels);
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
        renderer.prepareMeshShader(context_.RawPtr(), lodIndex);
        renderer.drawMeshlets(context_.RawPtr(), lodIndex);
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
        std::array<Uint32, 3> pixelCounts{};
        if (!mapped.pData) return pixelCounts;
        const auto* bytes = static_cast<const std::uint8_t*>(mapped.pData);
        for (Uint32 y = 0; y < height; ++y) {
            const auto* row = bytes + y * mapped.Stride;
            for (Uint32 x = 0; x < width; ++x) {
                float alpha = 0.0f;
                std::memcpy(&alpha, row + x * 4 * sizeof(float) + 3 * sizeof(float),
                            sizeof(alpha));
                if (alpha > 0.5f) {
                    const std::size_t band = x < width / 4 ? 0
                        : x < width / 2 ? 1 : 2;
                    ++pixelCounts[band];
                }
            }
        }
        context_->UnmapTextureSubresource(stagingTexture, 0, 0);
        return pixelCounts;
    };

    const auto fineLodPixels = countDrawnPixelsByBand(96.0f);
    EXPECT_GT(fineLodPixels[0], 0u);
    EXPECT_GT(fineLodPixels[1], 0u)
        << "the fine LOD should dispatch both of its meshlet groups";
    EXPECT_EQ(fineLodPixels[2], 0u)
        << "the fine LOD should draw only its left-side meshlets";
    const auto coarseLodPixels = countDrawnPixelsByBand(48.0f);
    EXPECT_EQ(coarseLodPixels[0], 0u)
        << "the coarse LOD should not draw the fine LOD meshlet";
    EXPECT_EQ(coarseLodPixels[1], 0u)
        << "the coarse LOD should not draw the second fine LOD meshlet";
    EXPECT_GT(coarseLodPixels[2], 0u)
        << "the coarse LOD should draw its right-side meshlet";
}

} // namespace
