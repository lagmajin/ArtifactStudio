// DRAFT — design artifact, NOT compiled, NOT registered in CMake.
//
// Seam: append this TEST to tests/Artifact/ClonerGpuRenderContractTest.cpp
// (already includes DiligentHeadlessRenderer.hpp and uses GTEST_SKIP() when
// headless Diligent is unavailable). No production-code change is proposed.
//
// Purpose: measure the actual layer(RGBA8 sRGB premultiplied) -> layerFloat
// conversion, whose sRGB decode is implicit in the sRGB SRV format and therefore
// backend-dependent (see docs/planned/MILESTONE_BUFFER_PRECISION_ALPHA_CHAIN_RD_2026-11-17.md, section 15).
//
// Exact helper APIs for ImageUploadBuffer byte access and the Core module import
// for LayerBlendPipeline must be confirmed against the target test file before
// this draft is turned into a real test.

#include <gtest/gtest.h>

#include <Texture.h>
#include <TextureView.h>
#include <DeviceContext.h>
#include <RenderDevice.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <future>
#include <vector>

#include "DiligentHeadlessRenderer.hpp"
// import Artifact.Render.Config;
// import the module that exports ArtifactCore::LayerBlendPipeline (confirm name).

namespace {

constexpr Diligent::Uint32 kWidth = 4;
constexpr Diligent::Uint32 kHeight = 1;

// Input bytes are premultiplied 8-bit sRGB values (BGRA layout is used by the
// production path). Asymmetric R/G/B makes a channel swap or a missing decode
// immediately visible.
std::array<unsigned char, kWidth * 4> makePremultipliedSrgbPixels()
{
    return {{
        // B,   G,   R,   A
        128, 64, 192, 255,   // opaque
        64, 32, 96, 128,     // half coverage
        0, 0, 0, 0,          // fully transparent
        192, 96, 255, 64     // low coverage, hidden RGB is zeroed by the shader
    }};
}

float srgbToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f
                             : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

} // namespace

TEST(LayerToFloatCaptureDraft, MeasuresActualSrgbAndAlphaOrder)
{
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.isAvailable()) GTEST_SKIP() << "headless Diligent unavailable";

    Diligent::TextureDesc srcDesc;
    srcDesc.Type = Diligent::TEXTURE_TYPE_2D;
    srcDesc.Width = kWidth;
    srcDesc.Height = kHeight;
    srcDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    srcDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    srcDesc.Usage = Diligent::USAGE_DEFAULT;
    Diligent::RefCntAutoPtr<Diligent::ITexture> srcTexture;
    renderer.device()->CreateTexture(srcDesc, nullptr, &srcTexture);
    ASSERT_TRUE(srcTexture);

    const auto pixels = makePremultipliedSrgbPixels();
    Diligent::TextureSubResData subRes{pixels.data(), kWidth * 4};
    renderer.context()->UpdateTextureSubresources(srcTexture, 0, 1, &subRes);

    Diligent::TextureDesc dstDesc = srcDesc;
    dstDesc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    dstDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS;
    Diligent::RefCntAutoPtr<Diligent::ITexture> dstTexture;
    renderer.device()->CreateTexture(dstDesc, nullptr, &dstTexture);
    ASSERT_TRUE(dstTexture);

    auto pipeline = renderer.createLayerBlendPipeline();
    ASSERT_TRUE(pipeline);
    ASSERT_TRUE(pipeline->initialize());

    ASSERT_TRUE(renderer.convertLayerToFloat(
        pipeline.get(),
        srcTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE),
        dstTexture->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS),
        kWidth, kHeight));

    std::promise<std::vector<float>> captured;
    auto future = captured.get_future();
    ASSERT_TRUE(renderer.readbackTextureViewToBufferAsync(
        dstTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE),
        [&](const ArtifactCore::SharedPtr<const ArtifactCore::ImageUploadBuffer>& buffer) {
            // TODO: confirm ImageUploadBuffer byte/span accessor, then copy
            // buffer size / 4 floats into captured.set_value(...).
        },
        true));

    const std::vector<float> actual = future.get();
    ASSERT_EQ(actual.size(), kWidth * 4u);

    for (Diligent::Uint32 x = 0; x < kWidth; ++x) {
        const float alpha = static_cast<float>(pixels[x * 4 + 3]) / 255.0f;
        // Reference: unpremultiply in the encoded domain, decode, then
        // premultiply in linear. Whether the GPU does this is the question.
        const float encoded = alpha > 0.0f
            ? static_cast<float>(pixels[x * 4 + 1]) / 255.0f / alpha
            : 0.0f;
        const float expected = srgbToLinear(encoded) * alpha;
        EXPECT_NEAR(actual[x * 4 + 1], expected, 1.0e-3f)
            << "x=" << x << " alpha=" << alpha;
    }
}
