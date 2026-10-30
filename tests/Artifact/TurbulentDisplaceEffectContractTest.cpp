#include <gtest/gtest.h>

#include <array>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.TurbulentDisplace;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 16;
constexpr int kHeight = 16;

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

std::array<float, kWidth * kHeight * 4> makeGradient()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int offset = (y * kWidth + x) * 4;
            pixels[offset] = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(kHeight - 1);
            pixels[offset + 2] = 0.35f;
            pixels[offset + 3] = 0.65f;
        }
    }
    return pixels;
}

void configure(TurbulentDisplaceEffect& effect, int seed)
{
    effect.setAmount(3.0f);
    effect.setSize(4.0f);
    effect.setOctaves(3);
    effect.setSeed(seed);
    effect.setDomainWarp(0.5f);
}

} // namespace

TEST(TurbulentDisplaceEffectContractTest, SeedControlsRepeatableMappingAndAlphaStaysStable)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache repeatedOutput;
    ImageF32x4RGBAWithCache otherSeedOutput;

    TurbulentDisplaceEffect firstEffect;
    configure(firstEffect, 41);
    firstEffect.applyCPUOnly(source, firstOutput);

    TurbulentDisplaceEffect repeatedEffect;
    configure(repeatedEffect, 41);
    repeatedEffect.applyCPUOnly(source, repeatedOutput);

    TurbulentDisplaceEffect otherSeedEffect;
    configure(otherSeedEffect, 42);
    otherSeedEffect.applyCPUOnly(source, otherSeedOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* repeated = repeatedOutput.image().rgba32fData();
    const float* otherSeed = otherSeedOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(otherSeed, nullptr);
    bool movedPixels = false;
    bool seedChangesMapping = false;
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        const int offset = pixel * 4;
        EXPECT_NEAR(first[offset], repeated[offset], 1.0e-6f);
        EXPECT_NEAR(first[offset + 1], repeated[offset + 1], 1.0e-6f);
        EXPECT_NEAR(first[offset + 3], 0.65f, 1.0e-6f);
        EXPECT_NEAR(repeated[offset + 3], 0.65f, 1.0e-6f);
        movedPixels = movedPixels ||
            std::abs(first[offset] - pixels[offset]) > 1.0e-4f ||
            std::abs(first[offset + 1] - pixels[offset + 1]) > 1.0e-4f;
        seedChangesMapping = seedChangesMapping ||
            std::abs(first[offset] - otherSeed[offset]) > 1.0e-4f ||
            std::abs(first[offset + 1] - otherSeed[offset + 1]) > 1.0e-4f;
    }
    EXPECT_TRUE(movedPixels);
    EXPECT_TRUE(seedChangesMapping);
    EXPECT_NEAR(source.image().getPixel(8, 8).r(), 8.0f / 15.0f, 1.0e-6f);
}

TEST(TurbulentDisplaceEffectContractTest, ZeroAmountPreservesEveryRgbaChannel)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    TurbulentDisplaceEffect effect;
    effect.setAmount(0.0f);
    effect.setSize(3.0f);
    effect.setOctaves(5);
    effect.setDomainWarp(1.0f);
    effect.setSeed(73);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(actual[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}
