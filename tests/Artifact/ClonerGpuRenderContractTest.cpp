#include <gtest/gtest.h>

#include <Texture.h>
#include <TextureView.h>
#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "DiligentHeadlessRenderer.hpp"
import Color.Float;
import Image.ImageF32x4_RGBA;

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

    Artifact::ClonerGridPlacementRange placementRange;
    ASSERT_TRUE(renderer.dispatchClonerGridPlacement(
        2, 1, 1, 40.0f, 0.0f, 0.0f, 30.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.75f,
        placementRange));
    ASSERT_EQ(placementRange.count, 3u);
    ASSERT_TRUE(placementRange.rotationAware);

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
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
    constexpr std::array<float, 3> expectedCenterX{44.0f, 64.0f, 84.0f};
    for (std::size_t index = 0; index < components.size(); ++index) {
        const auto& component = components[index];
        EXPECT_GT(component.pixels, 40u);
        EXPECT_LT(component.pixels, 400u);
        EXPECT_NEAR((component.left + component.right) * 0.5f,
                    expectedCenterX[index],
                    1.5f);
        EXPECT_NEAR((component.top + component.bottom) * 0.5f, 64.0f, 1.5f);
    }
}
