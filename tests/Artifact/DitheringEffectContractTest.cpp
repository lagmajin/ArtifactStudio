#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Dithering;
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

TEST(DitheringEffectContractTest, BayerDitheringQuantizesRgbAndPreservesAlpha)
{
    constexpr std::array<float, 4> pixels = {0.6f, 0.6f, 0.6f, 0.35f};
    const auto source = makeSurface(pixels.data(), 1, 1);
    ImageF32x4RGBAWithCache output;
    DitheringEffect effect;
    effect.setAlgorithm(DitherAlgorithm::Bayer4x4);
    effect.setColorCount(4);
    effect.setAmount(1.0f);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(0, 0);
    EXPECT_NEAR(pixel.r(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.5f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel.a(), 0.35f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).r(), 0.6f);
}

TEST(DitheringEffectContractTest, SettersClampRangesAndResetNonFiniteAmount)
{
    DitheringEffect effect;
    effect.setColorCount(1000);
    effect.setAmount(-1.0f);
    effect.setPatternScale(0.0f);

    EXPECT_EQ(effect.colorCount(), 256);
    EXPECT_FLOAT_EQ(effect.amount(), 0.0f);
    EXPECT_FLOAT_EQ(effect.patternScale(), 0.1f);

    effect.setColorCount(-10);
    effect.setAmount(std::numeric_limits<float>::infinity());
    effect.setPatternScale(std::numeric_limits<float>::quiet_NaN());

    EXPECT_EQ(effect.colorCount(), 2);
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);
    EXPECT_FLOAT_EQ(effect.patternScale(), 1.0f);
}

TEST(DitheringEffectContractTest, BayerTwoByTwoUsesOrderedThresholdRanks)
{
    constexpr std::array<float, 2 * 2 * 4> pixels = {
        0.6f, 0.6f, 0.6f, 0.1f,
        0.6f, 0.6f, 0.6f, 0.3f,
        0.6f, 0.6f, 0.6f, 0.7f,
        0.6f, 0.6f, 0.6f, 0.9f,
    };
    const auto source = makeSurface(pixels.data(), 2, 2);
    ImageF32x4RGBAWithCache output;
    DitheringEffect effect;
    effect.setAlgorithm(DitherAlgorithm::Bayer2x2);
    effect.setColorCount(4);
    effect.setAmount(1.0f);
    effect.setPatternScale(1.0f);

    effect.applyCPUOnly(source, output);

    constexpr float expectedRed[2][2] = {
        {0.5f, 0.5f},
        {1.0f, 0.5f},
    };
    constexpr float expectedAlpha[2][2] = {
        {0.1f, 0.3f},
        {0.7f, 0.9f},
    };
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), expectedRed[y][x]);
            EXPECT_FLOAT_EQ(pixel.g(), expectedRed[y][x]);
            EXPECT_FLOAT_EQ(pixel.b(), expectedRed[y][x]);
            EXPECT_FLOAT_EQ(pixel.a(), expectedAlpha[y][x]);
        }
    }
}

TEST(DitheringEffectContractTest, ZeroAmountRemovesOrderedPatternVariation)
{
    constexpr int width = 4;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.6f;
        pixels[pixel * 4 + 1] = 0.6f;
        pixels[pixel * 4 + 2] = 0.6f;
        pixels[pixel * 4 + 3] = 0.3f + 0.03f * static_cast<float>(pixel);
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    DitheringEffect effect;
    effect.setAlgorithm(DitherAlgorithm::Bayer4x4);
    effect.setColorCount(4);
    effect.setAmount(0.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), 0.5f);
            EXPECT_FLOAT_EQ(actual.g(), 0.5f);
            EXPECT_FLOAT_EQ(actual.b(), 0.5f);
            EXPECT_NEAR(actual.a(), source.image().getPixel(x, y).a(), 1.0e-6f);
        }
    }
}
