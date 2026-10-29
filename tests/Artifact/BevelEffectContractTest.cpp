#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Bevel;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(BevelEffectContractTest, BevelAddsRimAtAlphaBoundaryAndPreservesAlpha)
{
    constexpr int width = 9;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = 1.0f;
            pixels[offset + 1] = 1.0f;
            pixels[offset + 2] = 1.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    BevelEffect effect;
    effect.setStrength(1.0f);
    effect.setSoftness(1.0f);
    effect.setEdgeMode(false);

    effect.applyCPUOnly(source, output);

    bool boundaryBrightened = false;
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            boundaryBrightened = boundaryBrightened || pixel.r() > 1.0f + 1.0e-5f;
            EXPECT_NEAR(pixel.a(), 1.0f, 1.0e-6f);
        }
    }
    EXPECT_TRUE(boundaryBrightened);
    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 2).r(), 1.0f, 1.0e-6f);
}


TEST(BevelEffectContractTest, ZeroStrengthLeavesValidRgbaPixelsUnchanged)
{
    constexpr int width = 4;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int index = 0; index < width * height; ++index) {
        pixels[index * 4] = static_cast<float>(index) / (width * height - 1);
        pixels[index * 4 + 1] = 0.8f - static_cast<float>(index) / 20.0f;
        pixels[index * 4 + 2] = 0.25f;
        pixels[index * 4 + 3] = static_cast<float>(index % 4) / 3.0f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    BevelEffect effect;
    effect.setStrength(0.0f);
    effect.setSoftness(2.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(BevelEffectContractTest, SettersClampRangesAndResetNonFiniteValues)
{
    BevelEffect effect;
    effect.setStrength(8.0f);
    effect.setSoftness(-1.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 5.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 0.0f);

    effect.setStrength(std::numeric_limits<float>::quiet_NaN());
    effect.setSoftness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.strength(), 1.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 2.0f);
}
