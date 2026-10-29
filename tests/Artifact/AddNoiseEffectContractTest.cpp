#include <gtest/gtest.h>

#include <array>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.AddNoise;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

std::array<float, kWidth * kHeight * 4> makeGrayPixels()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        pixels[pixel * 4] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = 0.6f;
    }
    return pixels;
}

} // namespace

TEST(AddNoiseEffectContractTest, MonochromeNoiseIsSeededAndPreservesAlphaAndInput)
{
    const auto pixels = makeGrayPixels();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache repeatedOutput;
    ImageF32x4RGBAWithCache otherSeedOutput;

    AddNoiseEffect firstEffect;
    firstEffect.setAmount(0.4f);
    firstEffect.setSize(1.0f);
    firstEffect.setColorNoise(false);
    firstEffect.setMonochrome(true);
    firstEffect.setSeed(73);
    firstEffect.applyCPUOnly(source, firstOutput);

    AddNoiseEffect repeatedEffect;
    repeatedEffect.setAmount(0.4f);
    repeatedEffect.setSize(1.0f);
    repeatedEffect.setColorNoise(false);
    repeatedEffect.setMonochrome(true);
    repeatedEffect.setSeed(73);
    repeatedEffect.applyCPUOnly(source, repeatedOutput);

    AddNoiseEffect otherSeedEffect;
    otherSeedEffect.setAmount(0.4f);
    otherSeedEffect.setSize(1.0f);
    otherSeedEffect.setColorNoise(false);
    otherSeedEffect.setMonochrome(true);
    otherSeedEffect.setSeed(74);
    otherSeedEffect.applyCPUOnly(source, otherSeedOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* repeated = repeatedOutput.image().rgba32fData();
    const float* otherSeed = otherSeedOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(otherSeed, nullptr);
    bool changedByNoise = false;
    bool changedBySeed = false;
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        const int offset = pixel * 4;
        EXPECT_NEAR(first[offset], first[offset + 1], 1.0e-6f);
        EXPECT_NEAR(first[offset], first[offset + 2], 1.0e-6f);
        EXPECT_NEAR(first[offset], repeated[offset], 1.0e-6f);
        EXPECT_NEAR(first[offset + 3], 0.6f, 1.0e-6f);
        EXPECT_NEAR(repeated[offset + 3], 0.6f, 1.0e-6f);
        changedByNoise = changedByNoise || std::abs(first[offset] - 0.5f) > 1.0e-5f;
        changedBySeed = changedBySeed || std::abs(first[offset] - otherSeed[offset]) > 1.0e-5f;
    }
    EXPECT_TRUE(changedByNoise);
    EXPECT_TRUE(changedBySeed);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.5f, 1.0e-6f);
}

TEST(AddNoiseEffectContractTest, ZeroAmountPreservesRgbaExactly)
{
    const auto pixels = makeGrayPixels();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    AddNoiseEffect effect;
    effect.setAmount(0.0f);
    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t component = 0; component < pixels.size(); ++component) {
        EXPECT_FLOAT_EQ(actual[component], pixels[component])
            << "component=" << component;
    }
}

TEST(AddNoiseEffectContractTest, SizeGroupsNoiseIntoSharedPixelBlocks)
{
    const auto pixels = makeGrayPixels();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    AddNoiseEffect effect;
    effect.setAmount(0.4f);
    effect.setSize(2.0f);
    effect.setColorNoise(false);
    effect.setMonochrome(true);
    effect.setSeed(31);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    bool adjacentBlocksDiffer = false;
    for (int y = 0; y < kHeight; y += 2) {
        for (int x = 0; x < kWidth; x += 2) {
            const float blockValue = actual[(y * kWidth + x) * 4];
            EXPECT_FLOAT_EQ(actual[(y * kWidth + x + 1) * 4], blockValue);
            EXPECT_FLOAT_EQ(actual[((y + 1) * kWidth + x) * 4], blockValue);
            EXPECT_FLOAT_EQ(actual[((y + 1) * kWidth + x + 1) * 4], blockValue);
            EXPECT_FLOAT_EQ(actual[(y * kWidth + x) * 4 + 3], 0.6f);
            if (x + 2 < kWidth) {
                adjacentBlocksDiffer = adjacentBlocksDiffer ||
                    std::abs(actual[(y * kWidth + x + 2) * 4] - blockValue) > 1.0e-5f;
            }
        }
    }
    EXPECT_TRUE(adjacentBlocksDiffer);
}
