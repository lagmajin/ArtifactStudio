#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.DataMosh;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 2;
constexpr int kHeight = 2;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectPixelsEqual(
    const ArtifactCore::ImageF32x4RGBAWithCache& image,
    const Pixels& expected)
{
    const float* actual = image.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < expected.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel]);
    }
}

} // namespace

TEST(DataMoshEffectContractTest, ZeroBlendPreservesInputEvenWithHeldMoshBlocks)
{
    Artifact::DataMoshEffect effect;
    effect.setIntensity(1.0f);
    effect.setHoldFrames(4);
    effect.setBlockSize(4);
    effect.setBlend(0.0f);

    const Pixels firstFrame{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.2f, 0.3f, 0.4f, 0.5f,
        0.3f, 0.4f, 0.5f, 0.6f,
        0.4f, 0.5f, 0.6f, 0.7f,
    };
    const Pixels secondFrame{
        0.9f, 0.8f, 0.7f, 0.6f,
        0.8f, 0.7f, 0.6f, 0.5f,
        0.7f, 0.6f, 0.5f, 0.4f,
        0.6f, 0.5f, 0.4f, 0.3f,
    };
    ArtifactCore::ImageF32x4RGBAWithCache ignored;
    ArtifactCore::ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(makeImage(firstFrame), ignored);
    effect.applyCPUOnly(makeImage(secondFrame), output);

    expectPixelsEqual(output, secondFrame);
}

TEST(DataMoshEffectContractTest, ZeroIntensityPreservesSource)
{
    Artifact::DataMoshEffect effect;
    effect.setIntensity(0.0f);
    effect.setBlend(1.0f);
    const Pixels source{
        0.15f, 0.25f, 0.35f, 0.45f,
        0.25f, 0.35f, 0.45f, 0.55f,
        0.35f, 0.45f, 0.55f, 0.65f,
        0.45f, 0.55f, 0.65f, 0.75f,
    };
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(source), output);

    expectPixelsEqual(output, source);
}

TEST(DataMoshEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::DataMoshEffect effect;
    effect.setIntensity(-1.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 0.0f);
    effect.setIntensity(2.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 1.0f);
    effect.setHoldFrames(0);
    EXPECT_EQ(effect.holdFrames(), 1);
    effect.setHoldFrames(200);
    EXPECT_EQ(effect.holdFrames(), 120);
    effect.setBlockSize(0);
    EXPECT_EQ(effect.blockSize(), 4);
    effect.setBlockSize(100);
    EXPECT_EQ(effect.blockSize(), 64);
    effect.setBlend(-1.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 0.0f);
    effect.setBlend(2.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 1.0f);
}