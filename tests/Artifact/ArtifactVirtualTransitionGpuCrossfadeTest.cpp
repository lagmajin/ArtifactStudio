#include <gtest/gtest.h>

#include <Texture.h>
#include <TextureView.h>
#include <RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#include <QColor>
#include <QImage>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "DiligentHeadlessRenderer.hpp"

import Artifact.Render.IRenderer;
import Graphics.LayerBlendPipeline;

TEST(ArtifactVirtualTransitionGpuCrossfadeTest,
     InterpolatesPremultipliedColorAndAlphaAtAllProgressEndpoints)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(4, 4)) {
        GTEST_SKIP() << "Diligent headless renderer is unavailable on this host";
    }

    constexpr Diligent::Uint32 width = 4;
    constexpr Diligent::Uint32 height = 4;
    constexpr std::size_t pixelCount = width * height;
    // Layer blend surfaces use canonical straight-alpha BGRA storage; the
    // crossfade kernel consumes that renderer boundary and returns it intact.
    const std::array<float, 4> leftPixel = {0.0f, 0.0f, 0.25f, 0.5f};
    const std::array<float, 4> rightPixel = {0.75f, 0.0f, 0.0f, 1.0f};
    std::vector<float> leftPixels(pixelCount * 4u);
    std::vector<float> rightPixels(pixelCount * 4u);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        std::copy(leftPixel.begin(), leftPixel.end(),
                  leftPixels.begin() + static_cast<std::ptrdiff_t>(pixel * 4u));
        std::copy(rightPixel.begin(), rightPixel.end(),
                  rightPixels.begin() + static_cast<std::ptrdiff_t>(pixel * 4u));
    }

    const auto makeSource = [&](const char* name,
                                const std::vector<float>& pixels) {
        Diligent::TextureDesc desc;
        desc.Name = name;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData subresource;
        subresource.pData = pixels.data();
        subresource.Stride = width * 4u * sizeof(float);
        Diligent::TextureData initialData;
        initialData.pSubResources = &subresource;
        initialData.NumSubresources = 1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        renderer.device()->CreateTexture(desc, &initialData, &texture);
        return texture;
    };

    auto leftTexture = makeSource("Virtual transition left source", leftPixels);
    auto rightTexture = makeSource("Virtual transition right source", rightPixels);
    ASSERT_NE(leftTexture, nullptr);
    ASSERT_NE(rightTexture, nullptr);

    Diligent::TextureDesc outputDesc;
    outputDesc.Name = "Virtual transition crossfade output";
    outputDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    outputDesc.Width = width;
    outputDesc.Height = height;
    outputDesc.MipLevels = 1;
    outputDesc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    outputDesc.Usage = Diligent::USAGE_DEFAULT;
    outputDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE |
                           Diligent::BIND_UNORDERED_ACCESS;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTexture;
    renderer.device()->CreateTexture(outputDesc, nullptr, &outputTexture);
    ASSERT_NE(outputTexture, nullptr);
    auto* outputUAV = outputTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);
    auto* outputSRV = outputTexture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_NE(outputUAV, nullptr);
    ASSERT_NE(outputSRV, nullptr);

    auto blendPipeline = renderer.createLayerBlendPipeline();
    ASSERT_NE(blendPipeline, nullptr);
    ASSERT_TRUE(blendPipeline->initialize());
    const ArtifactCore::ComputeRegion region{0, 0, width, height};
    const auto renderAt = [&](const float progress) {
        EXPECT_TRUE(renderer.crossfadeLayers(
            blendPipeline.get(),
            leftTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE),
            rightTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE),
            outputUAV, progress, region));
        renderer.flushAndWait();
        return renderer.readbackTextureViewToImage(
            outputSRV, ArtifactCore::SurfaceChannelOrder::BGRA);
    };

    const QImage atStart = renderAt(0.0f);
    ASSERT_FALSE(atStart.isNull());
    const QColor startPixel = atStart.pixelColor(1, 1);
    EXPECT_NEAR(startPixel.red(), 137, 2);
    EXPECT_NEAR(startPixel.blue(), 0, 1);
    EXPECT_NEAR(startPixel.alpha(), 128, 1);

    const QImage atMiddle = renderAt(0.5f);
    ASSERT_FALSE(atMiddle.isNull());
    const QColor middlePixel = atMiddle.pixelColor(1, 1);
    EXPECT_NEAR(middlePixel.red(), 81, 2);
    EXPECT_NEAR(middlePixel.green(), 0, 1);
    EXPECT_NEAR(middlePixel.blue(), 188, 2);
    EXPECT_NEAR(middlePixel.alpha(), 191, 1);

    const QImage atEnd = renderAt(1.0f);
    ASSERT_FALSE(atEnd.isNull());
    const QColor endPixel = atEnd.pixelColor(1, 1);
    EXPECT_NEAR(endPixel.red(), 0, 1);
    EXPECT_NEAR(endPixel.blue(), 225, 2);
    EXPECT_NEAR(endPixel.alpha(), 255, 1);
}
