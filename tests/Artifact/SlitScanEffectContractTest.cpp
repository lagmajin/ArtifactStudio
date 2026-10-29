#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.SlitScan;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 1;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SlitScanEffectContractTest, AdvancesAutoSlitAndDecaysEarlierSlices)
{
    Artifact::SlitScanEffect effect;
    effect.setDirection(0.0f);
    effect.setSpeed(1.0f);
    effect.setPersistence(0.5f);
    effect.setPosition(-1.0f);

    const Pixels firstFrame{
        0.8f, 0.4f, 0.2f, 1.0f,
        0.1f, 0.2f, 0.3f, 1.0f,
        0.4f, 0.5f, 0.6f, 1.0f,
    };
    const Pixels secondFrame{
        0.2f, 0.3f, 0.4f, 1.0f,
        0.6f, 0.7f, 0.8f, 1.0f,
        0.9f, 0.8f, 0.7f, 1.0f,
    };
    ArtifactCore::ImageF32x4RGBAWithCache ignored;
    ArtifactCore::ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(makeImage(firstFrame), ignored);
    effect.applyCPUOnly(makeImage(secondFrame), output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.2f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.1f, 1.0e-6f);
    EXPECT_FLOAT_EQ(result[3], 0.5f);
    EXPECT_FLOAT_EQ(result[4], 0.6f);
    EXPECT_FLOAT_EQ(result[5], 0.7f);
    EXPECT_FLOAT_EQ(result[6], 0.8f);
    EXPECT_FLOAT_EQ(result[7], 1.0f);
    EXPECT_FLOAT_EQ(result[8], 0.0f);
    EXPECT_FLOAT_EQ(result[9], 0.0f);
    EXPECT_FLOAT_EQ(result[10], 0.0f);
    EXPECT_FLOAT_EQ(result[11], 0.0f);
}

TEST(SlitScanEffectContractTest, VerticalDirectionWritesSelectedRowAcrossWidth)
{
    Artifact::SlitScanEffect effect;
    effect.setDirection(1.0f);
    effect.setSpeed(0.0f);
    effect.setPersistence(0.0f);
    effect.setPosition(1.0f);
    constexpr std::array<float, 24> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.2f, 0.3f, 0.4f, 0.5f,
        0.3f, 0.4f, 0.5f, 0.6f,
        0.7f, 0.6f, 0.5f, 0.4f,
        0.6f, 0.5f, 0.4f, 0.3f,
        0.5f, 0.4f, 0.3f, 0.2f,
    };
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 2);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(ArtifactCore::ImageF32x4RGBAWithCache(image), output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < 12; ++channel) {
        EXPECT_FLOAT_EQ(result[channel], 0.0f);
    }
    for (std::size_t channel = 0; channel < 12; ++channel) {
        EXPECT_FLOAT_EQ(result[12 + channel], pixels[12 + channel]);
    }
}

TEST(SlitScanEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::SlitScanEffect effect;
    effect.setDirection(-1.0f);
    EXPECT_FLOAT_EQ(effect.direction(), 0.0f);
    effect.setDirection(2.0f);
    EXPECT_FLOAT_EQ(effect.direction(), 1.0f);

    effect.setSpeed(-1.0f);
    EXPECT_FLOAT_EQ(effect.speed(), 0.0f);
    effect.setPersistence(-1.0f);
    EXPECT_FLOAT_EQ(effect.persistence(), 0.0f);
    effect.setPersistence(2.0f);
    EXPECT_FLOAT_EQ(effect.persistence(), 1.0f);
}