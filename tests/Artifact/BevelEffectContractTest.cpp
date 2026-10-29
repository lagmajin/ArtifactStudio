#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Bevel;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 7;
constexpr int kHeight = 7;
using Pixels = std::array<float, kWidth * kHeight * 4>;

std::size_t pixelOffset(int x, int y)
{
    return (static_cast<std::size_t>(y) * kWidth + x) * 4;
}

ArtifactCore::ImageF32x4RGBAWithCache makeSurface(const Pixels& rgba)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

Pixels makeAlphaSquare()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = pixelOffset(x, y);
            pixels[offset] = 0.2f;
            pixels[offset + 1] = 0.2f;
            pixels[offset + 2] = 0.2f;
            pixels[offset + 3] =
                (x >= 2 && x <= 4 && y >= 2 && y <= 4) ? 1.0f : 0.0f;
        }
    }
    return pixels;
}

} // namespace

TEST(BevelEffectContractTest, ZeroStrengthPreservesEveryRgbaPixel)
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<float>(i) / static_cast<float>(pixels.size());
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BevelEffect effect;
    effect.setStrength(0.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const std::size_t offset = pixelOffset(x, y);
            EXPECT_FLOAT_EQ(actual.r(), pixels[offset]);
            EXPECT_FLOAT_EQ(actual.g(), pixels[offset + 1]);
            EXPECT_FLOAT_EQ(actual.b(), pixels[offset + 2]);
            EXPECT_FLOAT_EQ(actual.a(), pixels[offset + 3]);
        }
    }
}

TEST(BevelEffectContractTest, AlphaBoundaryReceivesHighlightAndKeepsAlpha)
{
    const auto source = makeSurface(makeAlphaSquare());
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BevelEffect effect;
    effect.setStrength(1.0f);
    effect.setSoftness(1.0f);

    effect.applyCPUOnly(source, output);

    float maximumRgb = 0.0f;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            maximumRgb = std::max({maximumRgb, actual.r(), actual.g(), actual.b()});
            const float expectedAlpha =
                (x >= 2 && x <= 4 && y >= 2 && y <= 4) ? 1.0f : 0.0f;
            EXPECT_FLOAT_EQ(actual.a(), expectedAlpha);
        }
    }
    EXPECT_GT(maximumRgb, 0.2f);
}

TEST(BevelEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::BevelEffect effect;
    effect.setStrength(-1.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 0.0f);
    effect.setStrength(6.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 5.0f);
    effect.setStrength(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.strength(), 1.0f);

    effect.setSoftness(-1.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 0.0f);
    effect.setSoftness(100.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 64.0f);
    effect.setSoftness(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.softness(), 2.0f);

    effect.setEdgeMode(true);
    EXPECT_TRUE(effect.edgeMode());
}
