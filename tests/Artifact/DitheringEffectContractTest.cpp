#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Dithering;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 2;
constexpr int kHeight = 2;

ArtifactCore::ImageF32x4RGBAWithCache makeGraySurface()
{
    constexpr std::array<float, kWidth * kHeight * 4> pixels{
        0.25f, 0.25f, 0.25f, 0.37f,
        0.25f, 0.25f, 0.25f, 0.41f,
        0.25f, 0.25f, 0.25f, 0.53f,
        0.25f, 0.25f, 0.25f, 0.67f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(DitheringEffectContractTest, BayerTwoByTwoUsesOrderedThresholdPattern)
{
    const auto source = makeGraySurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::DitheringEffect effect;
    effect.setAlgorithm(Artifact::DitherAlgorithm::Bayer2x2);
    effect.setColorCount(4);
    effect.setAmount(0.0f);
    effect.setPatternScale(0.5f);

    effect.applyCPUOnly(source, output);

    // At 25% gray with two levels, Bayer ranks [0,2;3,1] yield [0,0.5;0.5,0].
    constexpr std::array<float, kWidth * kHeight> expected{0.0f, 0.5f,
                                                            0.5f, 0.0f};
    constexpr std::array<float, kWidth * kHeight> expectedAlpha{
        0.37f, 0.41f, 0.53f, 0.67f};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * kWidth + x);
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), expected[index]);
            EXPECT_FLOAT_EQ(actual.g(), expected[index]);
            EXPECT_FLOAT_EQ(actual.b(), expected[index]);
            EXPECT_FLOAT_EQ(actual.a(), expectedAlpha[index]);
        }
    }
}

TEST(DitheringEffectContractTest, GpuPlanCarriesOrderedSettingsAndRejectsDiffusion)
{
    Artifact::DitheringEffect effect;
    effect.setAlgorithm(Artifact::DitherAlgorithm::Bayer2x2);
    effect.setColorCount(32);
    effect.setAmount(0.6f);
    effect.setPatternScale(1.5f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey,
              Artifact::gpuGenericKeyFromString("ordered_dither"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 0.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 32.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.6f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 1.5f);

    effect.setAlgorithm(Artifact::DitherAlgorithm::FloydSteinberg);
    EXPECT_FALSE(effect.appendGpuSpatialNodes(stack));
}

TEST(DitheringEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::DitheringEffect effect;
    effect.setAlgorithm(static_cast<Artifact::DitherAlgorithm>(99));
    EXPECT_EQ(effect.algorithm(), Artifact::DitherAlgorithm::Stucki);

    effect.setColorCount(1);
    EXPECT_EQ(effect.colorCount(), 2);
    effect.setColorCount(300);
    EXPECT_EQ(effect.colorCount(), 256);

    effect.setAmount(-1.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 0.0f);
    effect.setAmount(2.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);
    effect.setAmount(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);

    effect.setPatternScale(0.0f);
    EXPECT_FLOAT_EQ(effect.patternScale(), 0.1f);
    effect.setPatternScale(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.patternScale(), 1.0f);
}
